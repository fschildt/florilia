internal bool
alloc_and_init_bitmap(Bitmap *bitmap, s32 size)
{
    u8 *mem = malloc(size);
    if (!mem) return false;

    bitmap->mem = mem;
    bitmap->size = size;

    memset(bitmap->mem, 0, size);
    return true;
}

internal s32
alloc_bitmap_num(Bitmap *bitmap)
{
    u32 size = bitmap->size;

    // find byte
    // Todo: @PERFROMANCE maybe check multiple bytes at once
    s32 byte = 0;
    while (byte < size) {
        if (bitmap->mem[byte] != 0xff) break;
        byte++;
    }
    if (byte == size) return -1;

    // find bit
    s32 bit = 0;
    u8 val = bitmap->mem[byte];
    while (val & (1<<bit)) bit++;

    // update
    bitmap->mem[byte] |= 1<<bit;
    s32 num = byte*8 + bit;

    return num;
}

internal void
free_bitmap_num(Bitmap *bitmap, s32 num)
{
    u32 byte = num / 8;
    u32 bit  = num % 8;

    bitmap->mem[byte] ^= 1<<bit;
}
