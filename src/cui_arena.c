static inline uint64_t
_cui_get_alignment_offset(CuiArena *arena, uint64_t alignment)
{
    uint64_t alignment_offset = 0;
    uint64_t next_pointer     = (uint64_t) (arena->base + arena->occupied);
    uint64_t alignment_mask   = alignment - 1;

    if (next_pointer & alignment_mask)
    {
        alignment_offset = alignment - (next_pointer & alignment_mask);
    }

    return alignment_offset;
}

void
cui_arena_allocate(CuiArena *arena, uint64_t capacity)
{
    CuiClearStruct(*arena);

    arena->base = (uint8_t *) cui_platform_allocate(capacity);

    if (arena->base)
    {
        arena->capacity = capacity;
    }
}

void
cui_arena_deallocate(CuiArena *arena)
{
    if (arena->base)
    {
        cui_platform_deallocate(arena->base, arena->capacity);
        CuiClearStruct(*arena);
    }
}

void
cui_arena_initialize_with_memory(CuiArena *arena, void *memory, uint64_t size)
{
    if (memory && (size > 0))
    {
        arena->occupied = 0;
        arena->capacity = size;
        arena->base = (uint8_t *) memory;
        arena->temporary_memory_count = 0;
    }
}

void
cui_arena_clear(CuiArena *arena)
{
    CuiAssert(!arena->temporary_memory_count);
    arena->occupied = 0;
}

void *
cui_alloc(CuiArena *arena, uint64_t size, CuiAllocationParams params)
{
    void *result = 0;
    uint64_t alignment_offset = _cui_get_alignment_offset(arena, params.alignment);

    if ((arena->occupied + alignment_offset + size) <= arena->capacity)
    {
        result = arena->base + arena->occupied + alignment_offset;
        arena->occupied += (alignment_offset + size);

        if (params.clear)
        {
            uint8_t *dst = (uint8_t *) result;
            while (size--) *dst++ = 0;
        }
    }

    return result;
}

void *
cui_realloc(CuiArena *arena, void *old_pointer, uint64_t old_size, uint64_t size, CuiAllocationParams params)
{
    CuiAssert(size > old_size);

    void *result = 0;
    uint64_t alignment_offset = _cui_get_alignment_offset(arena, params.alignment);

    if (old_pointer)
    {
        void *next_pointer = arena->base + arena->occupied + alignment_offset;
        void *last_pointer = (uint8_t *) old_pointer + old_size;

        uint64_t remaining_size = size - old_size;

        if ((next_pointer == last_pointer) && (remaining_size < (arena->capacity - (arena->occupied + alignment_offset))))
        {
            cui_alloc(arena, remaining_size, params);
            result = old_pointer;
        }
        else
        {
            bool clear = params.clear;
            params.clear = false;

            result = cui_alloc(arena, size, params);

            CuiAssert(result);

            size -= old_size;

            uint8_t *src = (uint8_t *) old_pointer;
            uint8_t *dst = (uint8_t *) result;

            while (old_size--) *dst++ = *src++;

            if (clear)
            {
                while (size--) *dst++ = 0;
            }
        }
    }
    else
    {
        result = cui_alloc(arena, size, params);
    }

    return result;
}

CuiTemporaryMemory
cui_begin_temporary_memory(CuiArena *arena)
{
    CuiTemporaryMemory result;

    result.arena = arena;
    result.occupied = arena->occupied;
    result.index = arena->temporary_memory_count;

    arena->temporary_memory_count += 1;

    return result;
}

void
cui_end_temporary_memory(CuiTemporaryMemory temp_memory)
{
    CuiArena *arena = temp_memory.arena;

    arena->occupied = temp_memory.occupied;
    CuiAssert(arena->temporary_memory_count);
    arena->temporary_memory_count -= 1;
    CuiAssert(arena->temporary_memory_count == temp_memory.index);
}
