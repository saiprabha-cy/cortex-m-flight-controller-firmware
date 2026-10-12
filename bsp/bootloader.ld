ENTRY(Reset_Handler)
_Min_Stack_Size = 0x1000;
MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 32K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 128K - 32    /* last 32 bytes = shared handshake / boot info */
}
INCLUDE sections.ld
