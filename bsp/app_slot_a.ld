ENTRY(Reset_Handler)
_Min_Stack_Size = 0x1000;
MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08020200, LENGTH = 384K - 512   /* slot A + 512-byte image header */
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 128K - 32
}
INCLUDE sections.ld
