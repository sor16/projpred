# glmproj

An R package to perform projection predictive variable selection for generalized linear models fitted with [rstanarm][]. 

The package uses forward search starting from the empty submodel model, adds variables one at a time, each time choosing the variable that decreases the KL-divergence from the projection to the full model the most. 

Currently, the supported models (family objects in R) include Gaussian, Binomial and Poisson families.

Installation
------------

	if (!require(devtools)) {
  		install.packages("devtools")
  		library(devtools)
	}
	devtools::install_github('paasim/glmproj', ref='development')

    
Example
-------

    rm(list=ls())
    library(glmproj)
    library(rstanarm)
    options(mc.cores = parallel::detectCores())
    set.seed(1)

    # Gaussian and Binomial examples from the glmnet-package
    data('QuickStartExample', package = 'glmnet')
    #data('BinomialExample', package = 'glmnet') 

    # fit the full model with a sparsifying prior
    fit <- stan_glm(y ~ x, gaussian(), prior = hs(df = 1, global_scale=0.03), iter = 500, seed = 1)
    #fit <- stan_glm(y ~ x, binomial(), prior = hs(df = 1, global_scale=0.03), iter = 500, seed = 1)


    # perform the variable selection
    fit <- varsel(fit)
    
	# print the results
    varsel_summary(fit)

    # project the parameters for model sizes nv = 0,...,10 variables 
    fit <- project(fit_v, nv = c(0:10))
    proj_coef(fit)
    
    # perform cross-validation for the variable selection
    fit_cv <- cv_varsel(fit, cv_method='LOO')

    # plot the results
    varsel_plot(fit_cv)

References
------------
Dupuis, J. A. and Robert, C. P. (2003). Variable selection in qualitative models via an entropic explanatory power. Journal of Statistical Planning and Inference, 111(1-2):77–94.

Goutis, C. and Robert, C. P. (1998). Model choice in generalised linear models: a Bayesian approach via Kullback–Leibler projections. Biometrika, 85(1):29–37.

Juho Piironen and Aki Vehtari (2016). Comparison of Bayesian predictive methods for model selection. Statistics and Computing, ([online][piironenvehtari]).


  [rstanarm]: https://github.com/stan-dev/rstanarm
  [piironenvehtari]: https://link.springer.com/article/10.1007/s11222-016-9649-y

