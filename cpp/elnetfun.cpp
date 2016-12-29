#include <iostream>
#include <RcppArmadillo.h>
//[[Rcpp::depends(RcppArmadillo)]]


using namespace Rcpp;
using namespace arma;


/** Returns the value of the regularized quadratic approximation to the loss function
    that is to be minimized iteratively:
        L = 0.5*sum_i{ w_i (z_i - f_i)^2 } + lambda*{ 0.5*(1-alpha)*||beta||_2^2 + alpha*||beta||_1 }
 */
double loss_approx(vec beta,    // coefficients
                   vec f,       // latent values
                   vec z,       // locations for pseudo obsevations
                   vec w,       // weights of the pseudo observations (inverse-variances)
                   double lambda, // regularization parameter
                   double alpha)  // elastic net mixing parameter
{
	double loss;
	loss = 0.5*sum(w % square(z-f)) + lambda*( 0.5*(1-alpha)*sum(square(beta)) + alpha*(sum(abs(beta))) );
	return loss;
}


/** Updates the regression coefficients and the intercept (unless excluded) based on the
    current quadratic approximation to the loss function. This is done via the 'soft-thresholding'
    described by Friedman et. al (2009). Performs either one pass through the specified set
    of varibles or iterates until convergence.
*/
void coord_descent(	vec& beta, // regression coefficients
					double& beta0, // intercept
					vec& f, // latent values
					mat& x, // input matrix
					vec& z, // locations for pseudo obsevations
					vec& w, // weights of the pseudo observations (inverse-variances)
					double& lambda, // regularization parameter
					double& alpha, // elastic net mixing parameter
					bool intercept, // whether to use intercept
					std::set<int>& varind, // which coefficients are updated
					std::set<int>& active_set, // active set
					bool until_convergence, // true = until convergence, false = one pass through varind
					int& npasses, // counts total passes through the variables
					double thresh, // stop when relative change in the loss is smaller than this
					int maxiter = 1000) // maximum number of iterations (passes) through varind
{
	
	int iter = 0;
	double loss,loss_old;
	int k,j;
	double h;
	
	// initial loss
	loss_old = loss_approx(beta,f,z,w,lambda,alpha);
	
	// auxiliary that will be used later on
	double lam_alpha = lambda*alpha;
	double lam_oneminus_alpha = lambda*(1-alpha);
	
	while (iter < maxiter) {
		
		// update the intercept
		if (intercept) {
		    f = f - beta0;
		    beta0 = sum(w % (z - f)) / sum(w);
		    f = f + beta0;
		}
		
		active_set.clear();
		
		for (std::set<int>::iterator it=varind.begin(); it!=varind.end(); ++it) {
			
			// update the regression coefficients via 'soft-thresholding'
			
			// varible index
			j = *it;
			
			f = f - beta(j)*x.col(j);
			h = sum( w % x.col(j) % (z - f) ); // auxiliary variable
			
			if (fabs(h) <= lam_alpha) {
				beta(j) = 0.0;
			} else if (h > 0) {
				beta(j) = (h - lam_alpha) / ( sum(w % square(x.col(j))) + lam_oneminus_alpha );
		        active_set.insert(j);
			} else {
				beta(j) = (h + lam_alpha) / ( sum(w % square(x.col(j))) + lam_oneminus_alpha );
		        active_set.insert(j);
			}
			f = f + beta(j)*x.col(j);
		}
		
		++iter;
		++npasses;
		loss = loss_approx(beta,f,z,w,lambda,alpha);
		
		if (until_convergence) {
		    
		    if (fabs(loss-loss_old) / fabs(loss) < thresh) {
				break;
			} else {
				// continue iterating
				loss_old = loss;
			}
		} else {
			break;
		}
	}
	
	if (iter == maxiter)
		std::cout << "Warning: maximum number of iterations reached in coordinate descent. Results can be inaccurate!\n";
	
}



/** Computes the whole elastic-net regularization path given the grid of values to the lambda.
    Assumes that the lambda grid is selected carefully and utilizes the function pseudo_obs
    that returns the pseudo-observations corresponding to the quadratic approximation to the
    loss function for a given vector of latent values (see elnetfun.R).
 */
// [[Rcpp::export]]
List glm_elnet_c(mat x, // input matrix
               Function pseudo_obs, // R-function returning the pseudo-data based on the quadratic approximation
               vec lambda, // grid for the regularization parameter
               double alpha, // elastic net mixing parameter
               bool intercept, // whether to use intercept
               double thresh, // threshold for determining the convergence
               int qa_updates_max, // maximum for the total number of quadratic approximation updates
               int pmax, // stop computation when the active set size is equal or greater than this
               bool pmax_strict, // if true, then the active set size of the last beta is always at most pmax
               int as_updates_max = 50) // maximum number of active set updates for one quadratic approximation
{
	
    // for gaussian pseudo data
    List obs; 
    vec z; // observations
    vec w; // weights (inverse variances)
    
    int n = x.n_rows;
    int D = x.n_cols;
    int nlam = lambda.size();
    double lam; // temporary varible for fixed lambda
    int k; // lambda index
    int qau; // number of quadratic approximation updates
    int asu; // number of active set updates (for a given quadratic approximation)
    
    
    // for storing the whole solution path
    rowvec beta0_path(nlam);
    mat beta_path(D,nlam);
    beta0_path.zeros();
    beta_path.zeros();
    int npasses = 0; // counts how many times the coefficient vector is looped through
    urowvec qa_updates(nlam);
    qa_updates.zeros();
    urowvec as_updates(nlam);
    as_updates.zeros();
    
    
    // initialization
    vec beta(D);
    beta.zeros(D);
    double beta0 = 0.0;
    vec f = x*beta + beta0;
    std::set<int> active_set; 
    std::set<int> active_set_old;
    std::set<int> varind_all; // a constant set containing indices of all the variables
    for (int j=0; j<D; j++)
        varind_all.insert(j);
    
    
    obs = pseudo_obs(f);
    z = as<vec>(obs["z"]);
    w = as<vec>(obs["w"]);
    double loss_initial = loss_approx(beta, f, z, w, lambda(0), alpha); // initial loss
    double loss_old = loss_initial; // will be updated iteratively
    double loss; // will be updated iteratively
    
    // loop over lambda values
    for (k=0; k<nlam; ++k) {
        
        lam = lambda(k);
    	
        qau = 0;
        while (qau < qa_updates_max) {
            
            // update the quadratic likelihood approximation (would be needed only
            // for likelihoods other than gaussian) 
            obs = pseudo_obs(f);
            z = as<vec>(obs["z"]);
            w = as<vec>(obs["w"]);
            ++qau;
            
            // current value of the (approximate) loss function
            loss_old = loss_approx(beta, f, z, w, lam, alpha);
            
            // run the coordinate descent until convergence for the current
            //  quadratic approximation
            asu = 0;
            while (asu < as_updates_max) {

                // iterate within the current active set until convergence (this might update 
                // active_set_old, if some variable goes to zero)
                coord_descent(beta, beta0, f, x, z, w, lam, alpha, intercept, active_set, active_set_old, true, npasses, thresh);
                
                // perfom one pass over all the variables and check if the active set changes 
                // (this might update active_set)
                coord_descent(beta, beta0, f, x, z, w, lam, alpha, intercept, varind_all, active_set, false, npasses, thresh);
                
                ++asu;

				if (active_set==active_set_old) {
                    // active set did not change so convergence reached
                    // (for the current quadratic approximation to the loss function)
                    break;
                }
            }
            as_updates(k) = as_updates(k) + asu;
            
            // the loss after updating the coefficients
            loss = loss_approx(beta, f, z, w, lam, alpha);
            
            // check if converged
            if (fabs(loss-loss_old) / fabs(loss) < thresh) {
                // convergence reached; proceed to the next lambda value
                break;
            }
        }
        // store the current solution
        beta0_path(k) = beta0;
        beta_path.col(k) = beta;
        qa_updates(k) = qau;
        
        if (qau == qa_updates_max && qa_updates_max > 1)
        	std::cout << "glm_elnet warning: maximum number of quadratic approximation updates reached. Results can be inaccurate!\n";
        
        if (active_set.size() >= pmax || active_set.size() == D) {
			// obtained solution with more than pmax variables (or the number of columns in x), so terminate
			if (pmax_strict) {
			    // return solutions only up to the previous lambda value
			    beta0_path = beta0_path.head(k);
			    beta_path = beta_path.head_cols(k);
			} else {
			    // return solutions up to the current lambda value
			    beta0_path = beta0_path.head(k+1);
			    beta_path = beta_path.head_cols(k+1);
			}
			break;
        }
    }
    
    return List::create(beta_path, beta0_path, npasses, qa_updates, as_updates);
}























