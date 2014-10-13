#ifndef UINTBYTEARRAY_HLSL
#define UINTBYTEARRAY_HLSL

// Sets array[index] to value. Treats value a byte.
//--------------------------------------------------------------------------------------
void SetByteInUint(in out uint arr, in uint index, in uint value)
{
    // Convert index from bytes to bits
    index = index * 8;
    // Clear array[index]
    arr = arr & ~(0xFF << index);
    // Chop off everything except last byte of value
    value = value & 0xFF;
    // Save value to array[index]
    arr = arr | (value << index);
}

// Returns array[index] where index is offset of bytes
//--------------------------------------------------------------------------------------
uint GetByteInUint(in uint arr, in uint index)
{
    // Convert index from bytes to bits
    index = index * 8;
    // Turn array[index] to unsigned int
    return (arr & (0xFF << index)) >> index;
}

//--------------------------------------------------------------------------------------
uint Get2BitsInByte(in uint arr, in uint index)
{
    uint clearBits;
    // Convert index to bit offeset

#if (defined(STREAMING_DEBUG_OPTIONS) && (STREAMING_MAX_SURFACES_PER_PIXEL == 4))
    index = index * 3;
    clearBits = 0x7;
#else // !(defined(STREAMING_DEBUG_OPTIONS) && (STREAMING_MAX_SURFACES_PER_PIXEL == 4))
    index = index * 2;
    clearBits = 0x3;
#endif // !(defined(STREAMING_DEBUG_OPTIONS) && (STREAMING_MAX_SURFACES_PER_PIXEL == 4))

    // Turn array[index] to unsigned int
    return (arr & (clearBits << index)) >> index;
}

// Returns the 2 bits at index. For 1 byte, index is in the range (0, 4)
//--------------------------------------------------------------------------------------
void Set2BitsInByte(in out uint arr, in uint index, in uint value)
{
    uint clearBits;

    // Convert index to bit offeset
#if (defined(STREAMING_DEBUG_OPTIONS) && (STREAMING_MAX_SURFACES_PER_PIXEL == 4))
    index = index * 3;
    clearBits = 0x7;
#else // !(defined(STREAMING_DEBUG_OPTIONS) && (STREAMING_MAX_SURFACES_PER_PIXEL == 4))
    index = index * 2;
    clearBits = 0x3;
#endif // !(defined(STREAMING_DEBUG_OPTIONS) && (STREAMING_MAX_SURFACES_PER_PIXEL == 4))

    // Clear array[index]
    arr = arr & ~(clearBits << index);
    // Chop off everything except last byte of value
    value = value & clearBits;
    // Save value to array[index]
    arr = arr | (value << index);
}

#endif // UINTBYTEARRAY_HLSL