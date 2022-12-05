#ifndef KERNELS_H
#define KERNELS_H

#ifdef KERNEL_3x3
const uint kRadius = 1;
const uint kWidth = 1 + 2 * kRadius;
const float kernel1D[kWidth] = {0.27901, 0.44198, 0.27901};
const float kernel[kWidth][kWidth] = {
  {kernel1D[0] * kernel1D[0], kernel1D[0] * kernel1D[1], kernel1D[0] * kernel1D[2]},
  {kernel1D[1] * kernel1D[0], kernel1D[1] * kernel1D[1], kernel1D[1] * kernel1D[2]},
  {kernel1D[2] * kernel1D[0], kernel1D[2] * kernel1D[1], kernel1D[2] * kernel1D[2]},
};
#elif defined(KERNEL_5x5)
const uint kRadius = 2;
const uint kWidth = 1 + 2 * kRadius;
const float kernel1D[kWidth] = { 1. / 16, 1. / 4, 3. / 8, 1. / 4, 1. / 16 };
const float kernel[kWidth][kWidth] =
{
  { kernel1D[0] * kernel1D[0], kernel1D[0] * kernel1D[1], kernel1D[0] * kernel1D[2], kernel1D[0] * kernel1D[3], kernel1D[0] * kernel1D[4] },
  { kernel1D[1] * kernel1D[0], kernel1D[1] * kernel1D[1], kernel1D[1] * kernel1D[2], kernel1D[1] * kernel1D[3], kernel1D[1] * kernel1D[4] },
  { kernel1D[2] * kernel1D[0], kernel1D[2] * kernel1D[1], kernel1D[2] * kernel1D[2], kernel1D[2] * kernel1D[3], kernel1D[2] * kernel1D[4] },
  { kernel1D[3] * kernel1D[0], kernel1D[3] * kernel1D[1], kernel1D[3] * kernel1D[2], kernel1D[3] * kernel1D[3], kernel1D[3] * kernel1D[4] },
  { kernel1D[4] * kernel1D[0], kernel1D[4] * kernel1D[1], kernel1D[4] * kernel1D[2], kernel1D[4] * kernel1D[3], kernel1D[4] * kernel1D[4] },
};
#elif defined(KERNEL_7x7)
const uint kRadius = 3;
const uint kWidth = 1 + 2 * kRadius;
const float kernel1D[kWidth] = { 0.00598, 0.060626, 0.241843, 0.383103, 0.241843, 0.060626, 0.00598 };
#endif

#endif // KERNELS_H