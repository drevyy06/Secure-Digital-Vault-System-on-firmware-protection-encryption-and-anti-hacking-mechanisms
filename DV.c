#include <LPC214x.h>
#include <string.h>

__attribute__((section(".ARM.__at_0x7FC")))
const unsigned int CRP_KEY = 0x87654321;

#define KEY 0x5A
const unsigned char encoded_pin[4] = {0x6B,0x68,0x69,0x6E}; // XOR encoded primary PIN ("1234")

#define SECONDARY_PIN_ADDR 0x40004000  // Fixed RAM location for secondary PIN
char *SECONDARY_PIN = (char*)SECONDARY_PIN_ADDR;
char previous_pin[5] = "0000";

// Function prototypes
void init_uart0(void);
void init_gpio(void);
void init_timer0(void);
void uart_send_string(char *str);
void uart_putchar(char c);
char uart_getchar(void);
void read_pin(char *buf);
int verify_primary_pin(char *input);
int compare_pin(const char *a,const char *b);
void generate_secondary_pin(void);
void delay_ms(unsigned int ms);

int main()
{
    char input_pin[5];
    int secondary_active = 0;
    int attempts = 0;
    init_gpio();
    init_uart0();
    init_timer0();

    while(1)
    {
        if(attempts >= 3)   // LOCK CONDITION
        {
            uart_send_string("\r\nVAULT LOCKED - TOO MANY ATTEMPTS\r\n");
            IO0CLR = (1<<7);
            while(1); // locked until reset
        }

        if(secondary_active == 0)  // PRIMARY PIN PHASE
        {
            uart_send_string("\r\nEnter PRIMARY PIN: ");
            read_pin(input_pin);

            if(verify_primary_pin(input_pin))
            {
                uart_send_string("\r\nPrimary Access Granted\r\n");
                IO0SET = (1<<7);
                generate_secondary_pin();
                uart_send_string("Secondary PIN Generated\r\n");
                attempts = 0;
                secondary_active = 1;
            }
            else
            {
                attempts++;
                uart_send_string("\r\nWrong PIN\r\n");

                if(attempts < 3)
                {
                    uart_send_string("Attempts left: ");
                    uart_putchar('0' + (3-attempts));
                    uart_send_string("\r\n");
                }
            }
        }
        // SECONDARY PIN PHASE
        else
        {
            uart_send_string("\r\nEnter SECONDARY PIN: ");
            read_pin(input_pin);

            if(compare_pin(input_pin,SECONDARY_PIN))
            {
                uart_send_string("\r\nVault Unlocked\r\n");
                IO0SET = (1<<7);
                generate_secondary_pin();
                uart_send_string("New Secondary PIN Generated\r\n");
                attempts = 0;
            }
            else
            {
                attempts++;
                uart_send_string("\r\nWrong Secondary PIN\r\n");

                if(attempts < 3)
                {
                    uart_send_string("Attempts left: ");
                    uart_putchar('0' + (3-attempts));
                    uart_send_string("\r\n");
                }

                IO0CLR = (1<<7);
            }
        }
        delay_ms(1000);
    }
}

int verify_primary_pin(char *input)
{
    int i;
    char decoded;

    for(i=0;i<4;i++)
    {
        decoded = encoded_pin[i] ^ KEY;
        if(input[i] != decoded)
            return 0;
    }
    return 1;
}


void generate_secondary_pin()
{
    char new_pin[5];
    int i;
    int same;

    do
    {
        same = 1;
        for(i=0;i<4;i++)
        {
            unsigned int t = T0TC;
            char digit = ((t >> (i*3)) & 0x0F) % 10;
            new_pin[i] = '0' + digit;
            delay_ms(3);
            if(new_pin[i] != previous_pin[i])
                same = 0;
        }

    } while(same);

    for(i=0;i<4;i++)
    {
        SECONDARY_PIN[i] = new_pin[i];
        previous_pin[i]  = new_pin[i];
    }
    SECONDARY_PIN[4] = '\0';
    previous_pin[4]  = '\0';
}

void init_timer0()
{
    T0TCR = 0x02;
    T0PR  = 0x00;
    T0TCR = 0x01;
}

void init_gpio()
{
    PINSEL0 = 0x00000000;
    IO0DIR |= (1<<7);
    IO0CLR = (1<<7);
}

void init_uart0()
{
    PINSEL0 |= 0x00000005;
    U0LCR = 0x83;
    U0DLL = 97;
    U0DLM = 0;
    U0LCR = 0x03;
    U0FCR = 0x07;
}
char uart_getchar()
{
    while(!(U0LSR & 0x01));
    return U0RBR;
}
void uart_putchar(char c)
{
    while(!(U0LSR & 0x20));
    U0THR = c;
}
void uart_send_string(char *str)
{
    while(*str)
        uart_putchar(*str++);
}
void read_pin(char *buf)
{
    int i;

    for(i=0;i<4;i++)
    {
        buf[i] = uart_getchar();
        uart_putchar(buf[i]);
    }

    buf[4] = '\0';
    uart_putchar('\r');
    uart_putchar('\n');
}
int compare_pin(const char *a,const char *b)
{
    return (memcmp(a,b,4)==0);
}
void delay_ms(unsigned int ms)
{
    unsigned int i,j;

    for(i=0;i<ms;i++)
        for(j=0;j<3000;j++);
}
