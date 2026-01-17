import numpy as np
import matplotlib.pyplot as plt
from numpy.polynomial import Polynomial

# Standard sRGB to Linear conversion
def srgb_to_linear_exact(c):
    if c <= 0.04045:
        return c / 12.92
    else:
        return ((c + 0.055) / 1.055)**2.4

# Standard Linear to sRGB conversion
def linear_to_srgb_exact(c):
    if c <= 0.0031308:
        return c * 12.92
    else:
        return 1.055 * (c**(1/2.4)) - 0.055

# Generate 256-element lookup tables
def generate_srgb_luts(num_elements=256):
    srgb_to_linear_lut = np.zeros(num_elements)
    linear_to_srgb_lut = np.zeros(num_elements)

    for i in range(num_elements):
        # sRGB to Linear LUT
        srgb_val = i / (num_elements - 1)
        srgb_to_linear_lut[i] = srgb_to_linear_exact(srgb_val)

        # Linear to sRGB LUT (denser near 0 with x^2 mapping)
        linear_val = (i / (num_elements - 1))**2
        linear_to_srgb_lut[i] = linear_to_srgb_exact(linear_val)
    
    return srgb_to_linear_lut, linear_to_srgb_lut

# Linear interpolation using a lookup table
def interpolate_lut(value, lut, is_linear_to_srgb=False):
    num_elements = len(lut)
    
    # Clamp value to [0, 1] range
    clamped_value = max(0.0, min(1.0, value))

    # Scale value to LUT index range, considering x^2 mapping for linear_to_srgb
    if is_linear_to_srgb and clamped_value >= 0: # Ensure non-negative for sqrt
        scaled_value = np.sqrt(clamped_value) * (num_elements - 1)
    else:
        scaled_value = clamped_value * (num_elements - 1)
    
    # Get integer and fractional parts
    idx = int(scaled_value)
    frac = scaled_value - idx

    # Handle edge case for the last element
    if idx >= num_elements - 1: # Use >= to catch values that might round up to num_elements
        return lut[num_elements - 1]
    
    # Linear interpolation
    return lut[idx] * (1 - frac) + lut[idx + 1] * frac

# sRGB to Linear approximation using LUT
def srgb_to_linear_approx(c, lut):
    if c <= 0.04045:
        return c / 12.92
    else:
        return interpolate_lut(c, lut, is_linear_to_srgb=False)

# Linear to sRGB approximation using LUT
def linear_to_srgb_approx(c, lut):
    if c <= 0.0031308:
        return c * 12.92
    else:
        return interpolate_lut(c, lut, is_linear_to_srgb=True)

# Refined sRGB to Linear approximation using LUT and error polynomial
def srgb_to_linear_refined(c, lut, coeffs, num_elements):
    if c <= 0.04045:
        return c / 12.92
    else:
        # Clamp value to [0, 1] range
        clamped_value = max(0.0, min(1.0, c))

        # Determine segment index
        scaled_value = clamped_value * (num_elements - 1)
        idx = int(scaled_value)
        
        # Handle edge case for the last element
        if idx >= num_elements - 1:
            return interpolate_lut(c, lut, is_linear_to_srgb=False) # No error correction for the last point

        # Get linear approximation
        linear_approx = interpolate_lut(c, lut, is_linear_to_srgb=False)

        # Calculate normalized x for polynomial evaluation within the segment
        segment_start = idx / (num_elements - 1)
        segment_end = (idx + 1) / (num_elements - 1)
        
        # Avoid division by zero if segment_start == segment_end (shouldn't happen with num_elements > 1)
        if segment_end - segment_start == 0:
            normalized_x = 0
        else:
            normalized_x = 2 * (clamped_value - segment_start) / (segment_end - segment_start) - 1

        # Get polynomial for the current segment
        poly = Polynomial(coeffs[idx])
        error_correction = poly(normalized_x)
        
        return linear_approx - error_correction

# Refined Linear to sRGB approximation using LUT and error polynomial
def linear_to_srgb_refined(c, lut, coeffs, num_elements):
    if c <= 0.0031308:
        return c * 12.92
    else:
        # Clamp value to [0, 1] range
        clamped_value = max(0.0, min(1.0, c))

        # Determine segment index
        # For linear_to_srgb, the input 'c' is linear, and the LUT is generated with x^2 mapping.
        # So, to find the correct segment index, we need to 'undo' the x^2 mapping.
        if clamped_value >= 0:
            scaled_value_for_idx = np.sqrt(clamped_value) * (num_elements - 1)
        else:
            scaled_value_for_idx = 0 # Should not happen with clamped_value >= 0
        idx = int(scaled_value_for_idx)

        # Handle edge case for the last element
        if idx >= num_elements - 1:
            return interpolate_lut(c, lut, is_linear_to_srgb=True) # No error correction for the last point

        # Get linear approximation
        linear_approx = interpolate_lut(c, lut, is_linear_to_srgb=True)

        # Calculate normalized x for polynomial evaluation within the segment
        # The segment start/end for the polynomial fit corresponds to the *linear* values
        # that map to the LUT indices.
        segment_start_linear = (idx / (num_elements - 1))**2
        segment_end_linear = ((idx + 1) / (num_elements - 1))**2

        # Avoid division by zero if segment_start_linear == segment_end_linear
        if segment_end_linear - segment_start_linear == 0:
            normalized_x = 0
        else:
            normalized_x = 2 * (clamped_value - segment_start_linear) / (segment_end_linear - segment_start_linear) - 1

        # Get polynomial for the current segment
        poly = Polynomial(coeffs[idx])
        error_correction = poly(normalized_x)
        
        return linear_approx - error_correction

# Main execution
if __name__ == "__main__":
    num_elements = 256
    srgb_to_linear_lut, linear_to_srgb_lut = generate_srgb_luts(num_elements)

    # Store polynomial coefficients
    srgb_to_linear_coeffs = np.zeros((num_elements - 1, 5)) # 4-degree polynomial has 5 coefficients
    linear_to_srgb_coeffs = np.zeros((num_elements - 1, 5))

    samples_per_segment = 64

    for i in range(num_elements - 1):
        # Define the range for the current segment
        # sRGB to Linear segment
        srgb_segment_start = i / (num_elements - 1)
        srgb_segment_end = (i + 1) / (num_elements - 1)
        srgb_segment_values = np.linspace(srgb_segment_start, srgb_segment_end, samples_per_segment)

        # Calculate exact and approximate values for sRGB to Linear
        srgb_to_linear_exact_segment = np.array([srgb_to_linear_exact(v) for v in srgb_segment_values])
        srgb_to_linear_approx_segment = np.array([srgb_to_linear_approx(v, srgb_to_linear_lut) for v in srgb_segment_values])
        srgb_to_linear_error_segment = srgb_to_linear_approx_segment - srgb_to_linear_exact_segment

        # Fit polynomial to sRGB to Linear error
        # Normalize x values to [-1, 1] for better polynomial fitting
        normalized_srgb_x = 2 * (srgb_segment_values - srgb_segment_start) / (srgb_segment_end - srgb_segment_start) - 1
        poly_srgb_to_linear = Polynomial.fit(normalized_srgb_x, srgb_to_linear_error_segment, 4)
        srgb_to_linear_coeffs[i] = poly_srgb_to_linear.coef

        # Linear to sRGB segment (adjusted for x^2 mapping)
        linear_segment_start_idx = i / (num_elements - 1)
        linear_segment_end_idx = (i + 1) / (num_elements - 1)
        
        linear_segment_start = linear_segment_start_idx**2
        linear_segment_end = linear_segment_end_idx**2
        
        linear_segment_values = np.linspace(linear_segment_start, linear_segment_end, samples_per_segment)

        # Calculate exact and approximate values for Linear to sRGB
        linear_to_srgb_exact_segment = np.array([linear_to_srgb_exact(v) for v in linear_segment_values])
        linear_to_srgb_approx_segment = np.array([linear_to_srgb_approx(v, linear_to_srgb_lut) for v in linear_segment_values])
        linear_to_srgb_error_segment = linear_to_srgb_approx_segment - linear_to_srgb_exact_segment

        # Fit polynomial to Linear to sRGB error
        # Normalize x values to [-1, 1] for better polynomial fitting
        normalized_linear_x = 2 * (linear_segment_values - linear_segment_start) / (linear_segment_end - linear_segment_start) - 1
        poly_linear_to_srgb = Polynomial.fit(normalized_linear_x, linear_to_srgb_error_segment, 4)
        linear_to_srgb_coeffs[i] = poly_linear_to_srgb.coef

    # Plotting coefficients
    segment_indices = np.arange(num_elements - 1)

    plt.figure(figsize=(14, 10))

    # sRGB to Linear Coefficients
    plt.subplot(2, 1, 1)
    for j in range(5): # 5 coefficients for a 4-degree polynomial (c0, c1, c2, c3, c4)
        plt.plot(segment_indices, srgb_to_linear_coeffs[:, j], label=f'Coefficient {j}')
    plt.title('sRGB to Linear Error Polynomial Coefficients per Segment')
    plt.xlabel('Segment Index')
    plt.ylabel('Coefficient Value')
    plt.legend()
    plt.grid(True)

    # Linear to sRGB Coefficients
    plt.subplot(2, 1, 2)
    for j in range(5): # 5 coefficients
        plt.plot(segment_indices, linear_to_srgb_coeffs[:, j], label=f'Coefficient {j}')
    plt.title('Linear to sRGB Error Polynomial Coefficients per Segment')
    plt.xlabel('Segment Index')
    plt.ylabel('Coefficient Value')
    plt.legend()
    plt.grid(True)

    plt.tight_layout()
    plt.savefig('srgb_lut_polynomial_coefficients.png')
    # plt.show() # Commented out to avoid blocking execution

    # Calculate errors for refined approximations
    test_values_full_range = np.linspace(0.0, 1.0, 1000) # Test full range for refined approximations

    srgb_to_linear_refined_results = np.array([srgb_to_linear_refined(v, srgb_to_linear_lut, srgb_to_linear_coeffs, num_elements) for v in test_values_full_range])
    srgb_to_linear_exact_full_range = np.array([srgb_to_linear_exact(v) for v in test_values_full_range])
    srgb_to_linear_refined_error = np.abs(srgb_to_linear_refined_results - srgb_to_linear_exact_full_range)

    linear_to_srgb_refined_results = np.array([linear_to_srgb_refined(v, linear_to_srgb_lut, linear_to_srgb_coeffs, num_elements) for v in test_values_full_range])
    linear_to_srgb_exact_full_range = np.array([linear_to_srgb_exact(v) for v in test_values_full_range])
    linear_to_srgb_refined_error = np.abs(linear_to_srgb_refined_results - linear_to_srgb_exact_full_range)

    # Calculate non-refined errors for comparison
    srgb_to_linear_approx_full_range = np.array([srgb_to_linear_approx(v, srgb_to_linear_lut) for v in test_values_full_range])
    srgb_to_linear_non_refined_error = np.abs(srgb_to_linear_approx_full_range - srgb_to_linear_exact_full_range)

    linear_to_srgb_approx_full_range = np.array([linear_to_srgb_approx(v, linear_to_srgb_lut) for v in test_values_full_range])
    linear_to_srgb_non_refined_error = np.abs(linear_to_srgb_approx_full_range - linear_to_srgb_exact_full_range)

    # Plotting refined and non-refined errors for comparison
    plt.figure(figsize=(14, 7))

    plt.subplot(1, 2, 1)
    plt.plot(test_values_full_range, srgb_to_linear_refined_error, label='Refined Approximation')
    plt.plot(test_values_full_range, srgb_to_linear_non_refined_error, label='Non-Refined (Linear Interpolation)')
    plt.title('Absolute Error: sRGB to Linear Approximation')
    plt.xlabel('sRGB Value')
    plt.ylabel('Absolute Error (Log Scale)')
    plt.yscale('log') # Set y-axis to log scale
    plt.legend()
    plt.grid(True)

    plt.subplot(1, 2, 2)
    plt.plot(test_values_full_range, linear_to_srgb_refined_error, label='Refined Approximation')
    plt.plot(test_values_full_range, linear_to_srgb_non_refined_error, label='Non-Refined (Linear Interpolation)')
    plt.title('Absolute Error: Linear to sRGB Approximation')
    plt.xlabel('Linear Value')
    plt.ylabel('Absolute Error (Log Scale)')
    plt.yscale('log') # Set y-axis to log scale
    plt.legend()
    plt.grid(True)

    plt.tight_layout()
    plt.savefig('srgb_lut_refined_vs_non_refined_error_plot.png')
    plt.show()
