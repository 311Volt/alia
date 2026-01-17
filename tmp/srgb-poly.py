import numpy as np
import matplotlib.pyplot as plt
import argparse
from numpy.polynomial.polynomial import Polynomial

# The target precision: 1 LSB in a 16-bit unsigned integer range [0, 65535].
# An error greater than this value means the approximation is off by more than one level.
LSB_16_BIT_ERROR_THRESHOLD = 1.0 / 65535

# sRGB standard thresholds for the linear part of the curve.
SRGB_LINEAR_THRESHOLD = 0.04045
LINEAR_SRGB_THRESHOLD = 0.0031308

def find_best_piecewise_approximation(
    target_function, function_name, degree, pieces, domain=(0.0, 1.0),
    special_piece_degree=None, special_piece_boundary=None
):
    """
    Finds the best piecewise polynomial approximation for a target function using a least-squares fit
    over a specified domain, with an optional special configuration for the first piece.
    """
    domain_start, domain_end = domain
    print(f"--- Optimizing for {function_name} on domain [{domain_start:.6f}, {domain_end:.6f}] ---")
    print(f"Degree: {degree}, Pieces: {pieces}")
    
    has_special_piece = special_piece_degree is not None and pieces > 0
    if has_special_piece and special_piece_boundary is not None and pieces > 1:
        print(f"Special first piece: degree={special_piece_degree}, boundary={special_piece_boundary:.6f}")
    elif has_special_piece:
        print(f"Single piece with special degree: {special_piece_degree}")
    print()

    X_POINTS_GLOBAL = np.linspace(domain_start, domain_end, 8192)

    # Determine piece boundaries
    if has_special_piece and special_piece_boundary is not None and pieces > 1:
        if special_piece_boundary <= domain_start or special_piece_boundary >= domain_end:
            raise ValueError("Special piece boundary must be within the domain.")
        
        boundaries = [domain_start, special_piece_boundary]
        # The remaining `pieces - 1` pieces are in the rest of the domain.
        # We need `pieces-1` intervals, so `pieces` points for linspace.
        remaining_boundaries = np.linspace(special_piece_boundary, domain_end, pieces)
        boundaries.extend(remaining_boundaries[1:])
        piece_boundaries = np.array(boundaries)
    else:
        # Either no special piece, or only 1 piece total. In both cases, pieces are distributed uniformly.
        piece_boundaries = np.linspace(domain_start, domain_end, pieces + 1)

    y_constraint = target_function(domain_start)

    all_coeffs = []
    y_approx_full = np.zeros_like(X_POINTS_GLOBAL)
    max_error_overall = 0

    for i in range(pieces):
        x_start, x_end = piece_boundaries[i], piece_boundaries[i+1]
        
        current_degree = degree
        if i == 0 and has_special_piece:
            current_degree = special_piece_degree
        
        print(f"--- Piece {i+1}/{pieces} for domain [{x_start:.4f}, {x_end:.4f}] (degree {current_degree}) ---")

        x_points_piece = np.linspace(x_start, x_end, 2048)
        y_true_piece = target_function(x_points_piece)

        p_unconstrained = Polynomial.fit(x_points_piece, y_true_piece, current_degree)

        y_unconstrained_start = p_unconstrained(x_start)
        shift = y_constraint - y_unconstrained_start
        p_constrained = p_unconstrained + shift

        best_coeffs_poly = p_constrained.convert().coef
        all_coeffs.append(best_coeffs_poly)

        y_approx_piece = p_constrained(x_points_piece)
        error_piece = np.abs(y_true_piece - y_approx_piece)
        max_error_piece = np.max(error_piece)
        max_error_overall = max(max_error_overall, max_error_piece)

        print(f"Optimized Coefficients (c{current_degree} down to c0):")
        print(f"[{', '.join(f'{c:.12f}' for c in best_coeffs_poly[::-1])}]")
        print(f"Maximum Absolute Error in Piece: {max_error_piece:.12f}\n")

        piece_indices = (X_POINTS_GLOBAL >= x_start) & (X_POINTS_GLOBAL <= x_end)
        y_approx_full[piece_indices] = p_constrained(X_POINTS_GLOBAL[piece_indices])

        y_constraint = p_constrained(x_end)

    print("-" * 50)
    print("--- Overall Results ---")
    print(f"Maximum Absolute Error: {max_error_overall:.12f}")
    print(f"16-bit LSB Threshold:   {LSB_16_BIT_ERROR_THRESHOLD:.12f}")

    if max_error_overall < LSB_16_BIT_ERROR_THRESHOLD:
        print("\n✅ Precision target (1 LSB in 16-bit) is MET.")
    else:
        error_in_lsb = max_error_overall / LSB_16_BIT_ERROR_THRESHOLD
        print(f"\n❌ Precision target (1 LSB in 16-bit) is NOT MET.")
        print(f"   The maximum error is approx. {error_in_lsb:.2f} LSBs.")

    plt.figure(figsize=(12, 7))
    title = f'Error for {function_name} (p={pieces}, d={degree})'
    if has_special_piece:
        if pieces > 1:
            title = f'Error for {function_name} (p={pieces}, d1={special_piece_degree}, d_rest={degree})'
        else:
            title = f'Error for {function_name} (p=1, d={special_piece_degree})'

    error = target_function(X_POINTS_GLOBAL) - y_approx_full
    plt.plot(X_POINTS_GLOBAL, error, label='Error (True - Approx)')
    for boundary in piece_boundaries[1:-1]:
        plt.axvline(x=boundary, color='gray', linestyle=':', linewidth=0.8)
    plt.axhline(LSB_16_BIT_ERROR_THRESHOLD, color='r', linestyle='--', label='_nolegend_')
    plt.axhline(-LSB_16_BIT_ERROR_THRESHOLD, color='r', linestyle='--', label='16-bit LSB Threshold')
    plt.title(title)
    plt.xlabel('Input Value')
    plt.ylabel('Absolute Error')
    plt.legend()
    plt.grid(True)
    plt.show()

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Find best-fit piecewise polynomial for sRGB conversion functions.')
    parser.add_argument('--degree', type=int, default=5, help='Degree of the regular polynomial pieces.')
    parser.add_argument('--pieces', type=int, default=1, help='Total number of polynomial pieces.')
    parser.add_argument('--special-piece-degree', type=int, help='Degree of the first, special piece for linear-to-sRGB. If pieces=1, this is the degree for the single piece.')
    parser.add_argument('--special-piece-boundary', type=float, help='The end boundary for the first, special piece for linear-to-sRGB. Only used if pieces > 1.')
    args = parser.parse_args()

    srgb_to_linear_func = lambda x: ((x + 0.055) / 1.055)**2.4
    srgb_domain = (SRGB_LINEAR_THRESHOLD, 1.0)
    find_best_piecewise_approximation(
        srgb_to_linear_func, "sRGB to Linear", args.degree, args.pieces, domain=srgb_domain)

    print("\n" + "="*80 + "\n")

    linear_to_srgb_func = lambda x: 1.055 * x**(1/2.4) - 0.055
    linear_domain = (LINEAR_SRGB_THRESHOLD, 1.0)
    find_best_piecewise_approximation(
        linear_to_srgb_func, "Linear to sRGB", args.degree, args.pieces, domain=linear_domain,
        special_piece_degree=args.special_piece_degree,
        special_piece_boundary=args.special_piece_boundary)
