import matplotlib.pyplot as plt
import numpy as np

rng = np.random.default_rng()

def sample_with_censoring(alpha, beta, L, n_samples):
    T = rng.weibull(beta, n_samples) * alpha
    t = np.minimum(T, L)

    # sort here in order to compute the survival function easily later
    sorted_indexes = np.argsort(t)
    t = t[sorted_indexes]
    T = T[sorted_indexes]
    L = L[sorted_indexes]
    return T, t, L

def estimate_parameters_naively(x, L):
    b_hat = 1
    epsilon = 0.0001
    prev_b_hat = b_hat + 2*epsilon

    while abs(b_hat - prev_b_hat) > epsilon:
        prev_b_hat = b_hat
        new_b_hat = np.sum(x*np.exp(x/b_hat)) / np.sum(np.exp(x/b_hat)) - np.average(x)

        # Thie LPF adds stability to the b_hat computation, otherwise at high censoring percentages
        # the b_hat would become unstable or oscillate
        b_hat = new_b_hat * 0.1 + b_hat * 0.9

    beta_hat = 1 / b_hat

    alpha_hat = np.power(np.sum(np.exp(x/b_hat)) / n_samples, b_hat)
    u_hat = np.log(alpha_hat)

    return alpha_hat, beta_hat, u_hat, b_hat

def estimate_parameters_without_censoring(x, L):
    non_censored = x != np.log(L)
    x_w = x[non_censored]
    r = sum(non_censored)
    b_hat = 1
    epsilon = 0.0001
    prev_b_hat = b_hat + 2*epsilon

    while abs(b_hat - prev_b_hat) > epsilon:
        prev_b_hat = b_hat
        new_b_hat = np.sum(x_w*np.exp(x_w/b_hat)) / np.sum(np.exp(x_w/b_hat)) - np.average(x_w)

        # Thie LPF adds stability to the b_hat computation, otherwise at high censoring percentages
        # the b_hat would become unstable or oscillate
        b_hat = new_b_hat * 0.1 + b_hat * 0.9

    beta_hat = 1 / b_hat

    alpha_hat = np.power(np.sum(np.exp(x_w/b_hat)) / r, b_hat)
    u_hat = np.log(alpha_hat)

    return alpha_hat, beta_hat, u_hat, b_hat

def estimate_parameters_with_censoring(x, L):
    non_censored = x != np.log(L)
    r = sum(non_censored)
    non_censored_average = sum(x[non_censored]) / r

    b_hat = 1
    epsilon = 0.0001
    prev_b_hat = b_hat + 2*epsilon

    while abs(b_hat - prev_b_hat) > epsilon:
        prev_b_hat = b_hat
        new_b_hat = np.sum(x*np.exp(x/b_hat)) / np.sum(np.exp(x/b_hat)) - non_censored_average

        # Thie LPF adds stability to the b_hat computation, otherwise at high censoring percentages
        # the b_hat would become unstable or oscillate
        b_hat = new_b_hat * 0.01 + b_hat * 0.99

    beta_hat = 1 / b_hat

    alpha_hat = np.power(np.sum(np.exp(x/b_hat)) / r, b_hat)
    u_hat = np.log(alpha_hat)

    return alpha_hat, beta_hat, u_hat, b_hat

def estimate_parameters_with_S_hat(x, L):
    # Survival function
    x_deaths = x[x != np.log(L)]
    n = len(x) - np.linspace(0, len(x) - 1, n_samples)[x != np.log(L)]
    p = (n - 1) / n
    S_hat = np.fromiter((np.prod(p[0:i]) for i in range(len(p))), float)
    S_hat[0] = S_hat[1]
    
    coefficients = np.polyfit(x_deaths, np.log(-np.log(S_hat)), 1)
    
    beta_hat = coefficients[0]
    alpha_hat = np.exp(-coefficients[1]/beta_hat)
    u_hat = np.log(alpha_hat)
    b_hat = 1 / beta_hat

    return alpha_hat, beta_hat, u_hat, b_hat
    
def weib(t, alpha, beta):
    return (beta / alpha) *(t / alpha)**(beta - 1) *np.exp(-(t / alpha)**beta)


if __name__ == "__main__":

    fig, ax = plt.subplots(2, 2)

    alpha = 100
    beta = 1.5

    u = np.log(alpha)
    b = 1/beta

    n_samples = 1000
    L = 100 + rng.exponential(10, n_samples)
    T, t, L = sample_with_censoring(alpha, beta, L, n_samples)
    censoring_percentage = (1 - sum(T == t) / n_samples) * 100

    # Visual of the censoring
    ax[0, 0].scatter(T[t != L], t[t != L], label = 'Non - Censored')
    ax[0, 0].scatter(T[t == L], t[t == L], label = 'Censored')
    ax[0, 0].set_xlabel("T")
    ax[0, 0].set_ylabel("t")
    ax[0, 0].set_title(f"Censoring Percentage: {censoring_percentage:.2f} %")
    ax[0, 0].legend()

    x = np.log(t)

    # Estimate parameters
    alpha_hat_1, beta_hat_1, u_hat_1, b_hat_1 = estimate_parameters_naively(x, L)
    alpha_hat_2, beta_hat_2, u_hat_2, b_hat_2 = estimate_parameters_without_censoring(x, L)
    alpha_hat_3, beta_hat_3, u_hat_3, b_hat_3 = estimate_parameters_with_censoring(x, L)
    alpha_hat_4, beta_hat_4, u_hat_4, b_hat_4 = estimate_parameters_with_S_hat(x, L)

    # Compute distributions
    tt = np.linspace(T.min(), T.max(), 1000)
    ww_true    = weib(tt, alpha,       beta)
    ww_naive   = weib(tt, alpha_hat_1, beta_hat_1)
    ww_without = weib(tt, alpha_hat_2, beta_hat_2)
    ww_with    = weib(tt, alpha_hat_3, beta_hat_3)
    ww_S_hat   = weib(tt, alpha_hat_4, beta_hat_4)

    # Graph 1
    ax[1, 0].hist(T, label = 'True Samples', density = True)
    ax[1, 0].plot(tt, ww_true,  label = 'True Density')
    ax[1, 0].plot(tt, ww_with,  label = 'Estimated (With Censoring)')
    ax[1, 0].plot(tt, ww_S_hat, label = 'Estimated (With S_hat)')

    ax[1, 0].set_xlabel("t")
    ax[1, 0].set_ylabel("probability")
    ax[1, 0].legend()
    
    # Graph 2
    ax[1, 1].hist(t[t != L], label = 'Non Censored', density = True)
    ax[1, 1].hist(t,         label = 'Sampled',      density = True)

    ax[1, 1].plot(tt, ww_true,    label = 'True Density')
    ax[1, 1].plot(tt, ww_naive,   label = 'Estimated (Naively)')
    ax[1, 1].plot(tt, ww_without, label = 'Estimated (Without Censoring)')

    ax[1, 1].set_xlabel("t")
    ax[1, 1].set_ylabel("probability")
    ax[1, 1].legend()

    # Survival function
    S_hat_naive = (1 - np.linspace(0, len(t) - 1, n_samples) / len(t))
    S_hat_naive[0] = S_hat_naive[1]

    # This computation expects the ts to be sorted
    x_deaths = x[t != L]
    n = len(t) - np.linspace(0, len(t) - 1, n_samples)[t != L]
    p = (n - 1) / n
    S_hat = np.fromiter((np.prod(p[0:i]) for i in range(len(p))), float)
    S_hat[0] = S_hat[1]
    
    ax[0, 1].scatter(x,        np.log(-np.log(S_hat_naive)), label = 'Naive')
    ax[0, 1].scatter(x_deaths, np.log(-np.log(S_hat)),       label = 'With censoring')
    
    ax[0, 1].set_xlabel("x = log(t)")
    ax[0, 1].set_ylabel("log(-log(S_hat(t)))")
    ax[0, 1].legend()

    plt.show()

    # Todo: confidence interval