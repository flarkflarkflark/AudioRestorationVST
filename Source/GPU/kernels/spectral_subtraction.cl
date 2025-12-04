/**
 * OpenCL Kernels for GPU-Accelerated Spectral Noise Reduction
 *
 * Compatible with: AMD, NVIDIA, Intel, Apple GPUs
 *
 * These kernels perform spectral subtraction for noise reduction:
 * 1. Convert FFT output to magnitude/phase
 * 2. Subtract noise profile from magnitude
 * 3. Apply spectral floor to prevent musical noise
 * 4. Reconstruct complex signal with cleaned magnitude
 */

//==============================================================================
// Kernel 1: Magnitude extraction from complex FFT data
//==============================================================================
__kernel void extractMagnitude(
    __global const float2* fftData,    // Complex FFT output (real, imag pairs)
    __global float* magnitude,          // Output: magnitude spectrum
    __global float* phase,              // Output: phase spectrum
    const int numBins)                  // Number of frequency bins
{
    int idx = get_global_id(0);

    if (idx >= numBins)
        return;

    float2 complex = fftData[idx];
    float real = complex.x;
    float imag = complex.y;

    // Calculate magnitude: sqrt(real^2 + imag^2)
    magnitude[idx] = sqrt(real * real + imag * imag);

    // Calculate phase: atan2(imag, real)
    phase[idx] = atan2(imag, real);
}

//==============================================================================
// Kernel 2: Spectral subtraction with noise floor
//==============================================================================
__kernel void spectralSubtraction(
    __global const float* magnitude,       // Input: original magnitude spectrum
    __global const float* phase,           // Input: phase spectrum
    __global const float* noiseProfile,    // Input: noise profile to subtract
    __global float2* outputFFT,            // Output: cleaned complex FFT data
    const float reductionFactor,           // Noise reduction amount (linear)
    const float spectralFloor,             // Minimum allowed magnitude (prevents musical noise)
    const int numBins)                     // Number of frequency bins
{
    int idx = get_global_id(0);

    if (idx >= numBins)
        return;

    // Get original magnitude and noise estimate
    float origMag = magnitude[idx];
    float noiseMag = noiseProfile[idx] * reductionFactor;

    // Subtract noise (spectral subtraction)
    float cleanMag = origMag - noiseMag;

    // Apply spectral floor to prevent musical noise artifacts
    // Never reduce below spectralFloor * original magnitude
    float minMag = origMag * spectralFloor;
    cleanMag = fmax(cleanMag, minMag);

    // Reconstruct complex number with cleaned magnitude and original phase
    float p = phase[idx];
    float real = cleanMag * cos(p);
    float imag = cleanMag * sin(p);

    // Write cleaned complex FFT data
    outputFFT[idx] = (float2)(real, imag);
}

//==============================================================================
// Kernel 3: Optimized combined magnitude extraction + spectral subtraction
// (Single-pass optimization for better cache efficiency)
//==============================================================================
__kernel void spectralSubtractionFused(
    __global const float2* fftData,        // Input: complex FFT output
    __global const float* noiseProfile,    // Input: noise profile
    __global float2* outputFFT,            // Output: cleaned complex FFT
    const float reductionFactor,           // Noise reduction amount
    const float spectralFloor,             // Spectral floor
    const int numBins)                     // Number of bins
{
    int idx = get_global_id(0);

    if (idx >= numBins)
        return;

    // Load complex FFT data
    float2 complex = fftData[idx];
    float real = complex.x;
    float imag = complex.y;

    // Calculate magnitude and phase in one step
    float magnitude = sqrt(real * real + imag * imag);
    float phase = atan2(imag, real);

    // Spectral subtraction
    float noiseMag = noiseProfile[idx] * reductionFactor;
    float cleanMag = magnitude - noiseMag;

    // Apply spectral floor
    float minMag = magnitude * spectralFloor;
    cleanMag = fmax(cleanMag, minMag);

    // Reconstruct with cleaned magnitude
    float cleanReal = cleanMag * cos(phase);
    float cleanImag = cleanMag * sin(phase);

    // Write output
    outputFFT[idx] = (float2)(cleanReal, cleanImag);
}

//==============================================================================
// Kernel 4: Noise profile accumulation (for capturing noise profile)
//==============================================================================
__kernel void accumulateNoiseProfile(
    __global const float2* fftData,        // Input: FFT of noise section
    __global float* noiseProfile,          // Accumulator: running sum of magnitudes
    const int numBins)                     // Number of bins
{
    int idx = get_global_id(0);

    if (idx >= numBins)
        return;

    // Calculate magnitude
    float2 complex = fftData[idx];
    float magnitude = sqrt(complex.x * complex.x + complex.y * complex.y);

    // Accumulate (atomic add for thread safety across multiple frames)
    atomic_add((__global int*)&noiseProfile[idx], as_int(magnitude));
}

//==============================================================================
// Kernel 5: Normalize noise profile (divide by number of frames)
//==============================================================================
__kernel void normalizeNoiseProfile(
    __global float* noiseProfile,          // Input/Output: noise profile
    const int numFrames,                   // Number of frames accumulated
    const int numBins)                     // Number of bins
{
    int idx = get_global_id(0);

    if (idx >= numBins)
        return;

    // Average the accumulated magnitude
    noiseProfile[idx] /= (float)numFrames;
}

//==============================================================================
// Kernel 6: Apply window function (for overlap-add processing)
//==============================================================================
__kernel void applyWindow(
    __global float* audioData,             // Input/Output: audio samples
    __global const float* window,          // Input: window function (Hann, etc.)
    const int fftSize)                     // FFT size
{
    int idx = get_global_id(0);

    if (idx >= fftSize)
        return;

    audioData[idx] *= window[idx];
}

//==============================================================================
// Kernel 7: Overlap-add for reconstruction
//==============================================================================
__kernel void overlapAdd(
    __global const float* input,           // Input: current frame
    __global float* output,                // Input/Output: overlap buffer
    const int frameSize,                   // Frame size
    const int hopSize,                     // Hop size (for overlap)
    const int outputOffset)                // Offset in output buffer
{
    int idx = get_global_id(0);

    if (idx >= frameSize)
        return;

    int outIdx = outputOffset + idx;

    // Add current frame to output (overlapping with previous frames)
    atomic_add((__global int*)&output[outIdx], as_int(input[idx]));
}
