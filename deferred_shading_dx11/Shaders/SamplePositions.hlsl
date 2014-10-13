//--------------------------------------------------------------------------------------
// Copyright 2013 Intel Corporation
// All Rights Reserved
//
// Permission is granted to use, copy, distribute and prepare derivative works of this
// software for any purpose and without fee, provided, that the above copyright notice
// and this statement appear in all copies.  Intel makes no representations about the
// suitability of this software for any purpose.  THIS SOFTWARE IS PROVIDED "AS IS."
// INTEL SPECIFICALLY DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED, AND ALL LIABILITY,
// INCLUDING CONSEQUENTIAL AND OTHER INDIRECT DAMAGES, FOR THE USE OF THIS SOFTWARE,
// INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PROPRIETARY RIGHTS, AND INCLUDING THE
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  Intel does not
// assume any responsibility for any errors which may appear in this software nor any
// responsibility to update it.
//--------------------------------------------------------------------------------------

#ifndef SAMPLE_POSITIONS_H
#define SAMPLE_POSITIONS_H

#define SAMPLE_OFFSET(x, y) {((float)(x))/16.0f, ((float)(y))/16.0f}

static const float g1SampleOffsets[1][2] =
{
    SAMPLE_OFFSET(0, 0)
};

static const float g2SampleOffsets[2][2] =
{
    SAMPLE_OFFSET(4, 4),
    SAMPLE_OFFSET(-4, -4)
};

static const float g4SampleOffsets[4][2] =
{
    SAMPLE_OFFSET(-2, -6),
    SAMPLE_OFFSET(6, -2),
    SAMPLE_OFFSET(-6, 2),
    SAMPLE_OFFSET(2, 6)
};

static const float g8SampleOffsets[8][2] =
{
    SAMPLE_OFFSET(1, -3),
    SAMPLE_OFFSET(-1, 3),
    SAMPLE_OFFSET(5, 1),
    SAMPLE_OFFSET(-3, -5),
    SAMPLE_OFFSET(-5, 5),
    SAMPLE_OFFSET(-7, -1),
    SAMPLE_OFFSET(3, 7),
    SAMPLE_OFFSET(7, -7)
};

static const float g16SampleOffsets[16][2] =
{
    SAMPLE_OFFSET(1,1),
    SAMPLE_OFFSET(-1, -3),
    SAMPLE_OFFSET(-3, 2),
    SAMPLE_OFFSET(4, -1),
    SAMPLE_OFFSET(-5, -2),
    SAMPLE_OFFSET(2, 5),
    SAMPLE_OFFSET(5, 3),
    SAMPLE_OFFSET(3, -5),
    SAMPLE_OFFSET(-2, 6),
    SAMPLE_OFFSET(0, -7),
    SAMPLE_OFFSET(-4, -6),
    SAMPLE_OFFSET(-6, 4),
    SAMPLE_OFFSET(-8, 0),
    SAMPLE_OFFSET(7, -4),
    SAMPLE_OFFSET(6, 7),
    SAMPLE_OFFSET(-7, -8)
};

static const float g32SampleOffsets[32][2] =
{
    SAMPLE_OFFSET(0, -1),
    SAMPLE_OFFSET(2, 1),
    SAMPLE_OFFSET(-1, 2),
    SAMPLE_OFFSET(-3, 0),
    SAMPLE_OFFSET(3, -2),
    SAMPLE_OFFSET(-2, -3),
    SAMPLE_OFFSET(1, 4),
    SAMPLE_OFFSET(1, -4),
    SAMPLE_OFFSET(5, 0),
    SAMPLE_OFFSET(4, 3),
    SAMPLE_OFFSET(-4, 3),
    SAMPLE_OFFSET(-2, 5),
    SAMPLE_OFFSET(-5, -2),
    SAMPLE_OFFSET(-6, 1),
    SAMPLE_OFFSET(-1, -6),
    SAMPLE_OFFSET(4, -5),
    SAMPLE_OFFSET(-4, -5),
    SAMPLE_OFFSET(3, 6),
    SAMPLE_OFFSET(6, -3),
    SAMPLE_OFFSET(0, 7),
    SAMPLE_OFFSET(7, 2),
    SAMPLE_OFFSET(2, -7),
    SAMPLE_OFFSET(6, 5),
    SAMPLE_OFFSET(-5, 6),
    SAMPLE_OFFSET(-7, 4),
    SAMPLE_OFFSET(-7, -4),
    SAMPLE_OFFSET(-8, -1),
    SAMPLE_OFFSET(-3, -8),
    SAMPLE_OFFSET(7, -6),
    SAMPLE_OFFSET(-6, -7),
    SAMPLE_OFFSET(5, -8),
    SAMPLE_OFFSET(-8, 7)
};

float2 GetSamplePosition(int sampleId)
{
#if MSAA_SAMPLES == 2
    return float2(g2SampleOffsets[sampleId][0], g2SampleOffsets[sampleId][1]);
#elif MSAA_SAMPLES == 4
    return float2(g4SampleOffsets[sampleId][0], g4SampleOffsets[sampleId][1]);
#elif MSAA_SAMPLES == 8
    return float2(g8SampleOffsets[sampleId][0], g8SampleOffsets[sampleId][1]);
#endif // MSAA_SAMPLES == 8
}

#endif // SAMPLE_POSITIONS_H