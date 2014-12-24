#if !defined(__cplusplus)
#include <stdbool.h> /* C doesn't have booleans by default. */
#endif
#include <stddef.h>
#include <stdint.h>
 
/* Check if the compiler thinks if we are targeting the wrong operating system. */
#if defined(__linux__)
#error "You are not using a cross-compiler, you will most certainly run into trouble"
#endif
 
/* This tutorial will only work for the 32-bit ix86 targets. */
#if !defined(__i386__)
#error "This tutorial needs to be compiled with a ix86-elf compiler"
#endif
 
/* Hardware text mode color constants. */
enum vga_color
{
	COLOR_BLACK = 0,
	COLOR_BLUE = 1,
	COLOR_GREEN = 2,
	COLOR_CYAN = 3,
	COLOR_RED = 4,
	COLOR_MAGENTA = 5,
	COLOR_BROWN = 6,
	COLOR_LIGHT_GREY = 7,
	COLOR_DARK_GREY = 8,
	COLOR_LIGHT_BLUE = 9,
	COLOR_LIGHT_GREEN = 10,
	COLOR_LIGHT_CYAN = 11,
	COLOR_LIGHT_RED = 12,
	COLOR_LIGHT_MAGENTA = 13,
	COLOR_LIGHT_BROWN = 14,
	COLOR_WHITE = 15,
};
 
uint8_t make_color(enum vga_color fg, enum vga_color bg)
{
	return fg | bg << 4;
}
 
uint16_t make_vgaentry(char c, uint8_t color)
{
	uint16_t c16 = c;
	uint16_t color16 = color;
	return c16 | color16 << 8;
}
 
size_t strlen(const char* str)
{
	size_t ret = 0;
	while ( str[ret] != 0 )
		ret++;
	return ret;
}
 
static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
 
size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;
uint32_t* terminal_buffer;
 
void terminal_initialize()
{
	terminal_row = 0;
	terminal_column = 0;
	terminal_color = make_color(COLOR_LIGHT_GREY, COLOR_BLACK);
	terminal_buffer = (uint16_t*) 0xB8000;
	for ( size_t y = 0; y < VGA_HEIGHT; y++ )
	{
		for ( size_t x = 0; x < VGA_WIDTH; x++ )
		{
			const size_t index = y * VGA_WIDTH + x;
			terminal_buffer[index] = make_vgaentry(' ', terminal_color);
		}
	}
}
 
void terminal_setcolor(uint8_t color)
{
	terminal_color = color;
}
 
void terminal_putentryat(char c, uint8_t color, size_t x, size_t y)
{
	const size_t index = y * VGA_WIDTH + x;
	terminal_buffer[index] = make_vgaentry(c, color);
}
 
void terminal_putchar(char c)
{
	terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
	if ( ++terminal_column == VGA_WIDTH )
	{
		terminal_column = 0;
		if ( ++terminal_row == VGA_HEIGHT )
		{
			terminal_row = 0;
		}
	}
}
 
void terminal_writestring(const char* data)
{
	size_t datalen = strlen(data);
	for ( size_t i = 0; i < datalen; i++ )
		terminal_putchar(data[i]);
}

 #if defined(__cplusplus)
extern "C" /* Use C linkage for kernel_main. */
#endif

//------------------------start-------------------------
#define IRQ_KBD 0x01
#define KBD_BUF 0x60
#define STATUS_PORT 0x64

#define KBD_OUT_FULL 0x01
#define KBD_INPT_FULL 0x02
#define KBD_ENABLE 0xAE

inline char in_b(uint16_t port){
	char value;
	asm volatile("inb %1, %0":"=a" (value) : "dN" (port));
	return value;
}

inline void out_b(uint16_t port, char value){
	asm volatile("outb %0, %1"::"a"(value),"dN"(port));
}

void keyboard(){
	while((in_b(STATUS_PORT) & KBD_OUT_FULL));
	out_b(KBD_ENABLE, STATUS_PORT);

	while((in_b(STATUS_PORT) & KBD_INPT_FULL));
int i = 100;
while(i-- > 0){  // ???
		char st[2] ;
		st[0] = in_b(KBD_BUF);
		st[1]='\n';
		terminal_writestring(st);
	}
}
//---------------------finish--------------------------

//--------begin-----------------------------------------
void* memset(void *s, int c, size_t n)
{
char* p = s;
while(n--) *p++ = c;
return s;
}

uint8_t irq_base;
uint8_t irq_count;

#define IRQ_HANDLER(name) void name(); \
	asm(#name ": pusha \n call _" #name " \n movb $0x20, %al \n outb %al, $0x20 \n outb %al, $0xA0 \n popa \n iret"); \
	void _ ## name()

void init_interrupts();
void set_int_handler(uint8_t index, void *handler, uint8_t type);

typedef struct {
	uint16_t address_0_15;
	uint16_t selector;
	uint8_t reserved;
	uint8_t type;
	uint16_t address_16_31;
} __attribute__((packed)) IntDesc;

typedef struct {
	uint16_t limit;
	void *base;
} __attribute__((packed)) IDTR;

IntDesc *idt = (void*)0xFFFFC000;

void timer_int_handler();

void init_interrupts() {
	*((size_t*)0xFFFFEFF0) = 0x8000 | 3;
	memset(idt, 0, 256 * sizeof(IntDesc));
	IDTR idtr = {256 * sizeof(IntDesc), idt};
	asm("lidt (,%0,)"::"a"(&idtr));
	irq_base = 0x20;
	irq_count = 16;
	out_b(0x20, 0x11);
	out_b(0x21, irq_base);
	out_b(0x21, 4);
	out_b(0x21, 1);
	out_b(0xA0, 0x11);
	out_b(0xA1, irq_base + 8);
	out_b(0xA1, 2);
	out_b(0xA1, 1);
	set_int_handler(irq_base, timer_int_handler, 0x8E);
	asm("sti");
}

void set_int_handler(uint8_t index, void *handler, uint8_t type) {
	asm("pushf \n cli");
	idt[index].selector = 8;
	idt[index].address_0_15 = (size_t)handler & 0xFFFF;
	idt[index].address_16_31 = (size_t)handler >> 16;
	idt[index].type = type;
	idt[index].reserved = 0;
	asm("popf"); 
}
int16_t kk=0;
IRQ_HANDLER(timer_int_handler) {
	//(*((char*)(0xB8000 + 79 * 2)))++;
	terminal_writestring("interrupt handler");
	kk++;
	int16_t tmp = 10000;
	while(tmp>0){
	char c = kk/tmp;
	tmp=tmp/10;
	terminal_putchar(c);
}
}
//---------end------------------------------------------





void kernel_main()
{
	terminal_initialize();
	/* Since there is no support for newlines in terminal_putchar yet, \n will
	   produce some VGA specific character instead. This is normal. */
	terminal_writestring("Hello, kernel World!\n");
	asm volatile ("int $0x3");
asm volatile ("int $0x4");
	//keyboard();
        init_interrupts();
}
