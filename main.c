/************************************************************
*   PROYECTO FINAL MICROCONTROLADORES: main.c               *
*                                                           *
*   AUTOR: Yassin Achengli Benmouais                        *
*                                                           *
*   GRUPO: Ingenieria de Tecnologias de Telecomunicacion    *
*   PROYECTO EN GITHUB: #                                   *
*                                                           *
*************************************************************/

#include <msp430.h>
                                                                        // ESTADOS
#define ESTADO_SIMPLE       0
#define ESTADO_CONTINUO     1
#define ESTADO_REMOTO       2

#define TAM                 4                                           // TAMAÑO DE STRING DE CAMBIO DE ESTADO

#define ALTA                0                                           // NOTA ALTA O NOTA BAJA
#define BAJA                1

#define UNS                 2                                           // UNO O DOS SEGUNDOS -> PULSADOR
#define DOSS                4

#define FA4                 2863                                        // NOTAS BAJAS
#define MI4                 3034
#define RE4                 3405
#define DO4                 3822

#define DO5                 1911                                        // NOTAS ALTAS
#define SI4                 2025
#define LA4                 2273
#define SOL4                2551

void Config_UART (void);                                                // INSTANCIACIONES DE CONFIGURACION
void Config_Perif (void);
void Config_Timer (void);
void Config_WDT (void);
void Config_LPM (void);

void Config_UART (void) {
    UCA0CTL1    = UCSSEL_2;
    UCA0CTL1    &= ~UCSWRST;

    UCA0CTL0    &= ~(UC7BIT | UCSPB | UCPEN);
    UCA0CTL0    |= UCMODE_0;

    UCA0BR0     = 104;
    UCA0BR1     = 0;

    P1SEL       |= BIT1 | BIT2;
    P1SEL2      |= BIT1 | BIT2;

    UCA0MCTL     = (UCBRF_0 | UCBRS_1);

    IE2         |= (UCA0RXIE);
    IFG2        &= ~(UCA0RXIFG);
}

void Config_Perif (void) {
    P1SEL       = 0;
    P2SEL       = 0;
    P2SEL       |= BIT0;
    P1DIR       |= (BIT0 | BIT6);
    P1OUT       = 0;

    P1DIR       &= ~(BIT3 + BIT4);
    P1REN       |= (BIT3 + BIT4);
    P1OUT       |= (BIT3 + BIT4);
    P1IE        |= (BIT3 + BIT4);
    P1IES       |= (BIT3 + BIT4);
    P1IFG       &= ~(BIT3 + BIT4);

    P2DIR       |= (BIT0 + BIT4 + BIT5 + BIT6 + BIT7);
    P2OUT       &= ~(BIT0 + BIT4 + BIT5 + BIT6 + BIT7);
    P2DIR       &= ~(BIT1 + BIT2 + BIT3);
    P2REN       |= (BIT1 + BIT2 + BIT3);
    P2OUT       |= (BIT1 + BIT2 + BIT3);
    P2IE        |= (BIT1 + BIT2 + BIT3);
    P2IES       |= (BIT1 + BIT2 + BIT3);
    P2IFG       &= ~(BIT1 + BIT2 + BIT3);

}

void Config_Timer (void) {

    // Timer Zumbador
    TA1CTL      = (TASSEL_2 | MC_0);
    TA1CCR0     = 1135;
    TA1CCTL0    = OUTMOD_4;

    // Timer Pulsador
    TA0CTL      = (TASSEL_2 | MC_0 | ID_3 | TAIE);
    TA0CTL      &= ~TAIFG;
    TA0CCR0     = 62499;
    TA0CCTL0    |= CCIE;
    TA0CCTL0    &= ~CCIFG;

}

void Config_WDT (void) {
    WDTCTL      = (WDTPW | WDTHOLD);
    IE1         |= WDTIE;
    IFG1        &= ~WDTIFG;
}

void Config_LPM (void) {
    DCOCTL      = 0;
    BCSCTL1     = CALBC1_1MHZ;
    DCOCTL      = CALDCO_1MHZ;
}


volatile unsigned char estado, alta, cont, tiempo;                              // VARIABLES DE ENTORNO
volatile unsigned char                                                          // estado   --> estado del programa
    msg_SIMPLE[TAM] = {'-','S','M','T'},                                        // alta     --> identificador de nota alta
    msg_CONTINUO[TAM] = {'-','C','N','T'},                                      // cont     --> contador para tiempos
    msg_REMOTO[TAM] = {'-','R','M','T'};                                        // tiempo   --> para identificar 1s y 2s

void main (void) {

    Config_WDT ();                                                              // CONFIGURACIONES:
    Config_Perif ();                                                            // setear perifericos, uart, watchdog
    Config_LPM ();                                                              // low power mode y timer
    Config_UART ();
    Config_Timer ();

    alta = BAJA;                                                                // setear nota baja, estado simple y contador a 0
    estado = ESTADO_SIMPLE;
    cont = 0;

    __enable_interrupt ();                                                      // habilitar interrupciones

    while (1) { __low_power_mode_0 (); }                                        // Bucle infinito y bajo consumo LPM0
}
                                                                                /***********************
                                                                                *   MASCARA PUERTO 1   * 
                                                                                ************************/
#pragma vector = PORT1_VECTOR
__interrupt void RTI_PORT1 () {


    /************************
    *       PULSADOR S1     *
    *************************/

    if ((P1IFG & BIT3) == BIT3) {                                               // SI S1 ES PRESIONADO tiempo = 2s

        tiempo  = DOSS;
        P1IFG   &= ~BIT3;                                                       // Pongo S1 flanco de subida

        if ((P1IES & BIT3) == BIT3) {                                           // Si esta en flanco de bajada (acabo de presionarlo)
            TA0CTL  |= MC_1;
            TA0R    = 0;

            P1IES   &= ~BIT3;
        }
        else {                                                                  // Si esta en flanco de subida (he levantado el dedo)
            TA0CTL  &= ~MC_3;
            TA0R    = 0;

            P1IES   |= BIT3;

            if (cont >= 4) {                                                    // Compruebo si han pasado 2s
                if (estado == ESTADO_SIMPLE) {
                    estado  = ESTADO_CONTINUO;                                  // Si han pasado, permuto el estado, envio string
                    P1OUT   |= BIT0;                                            // por UART y permuto el led

                    IE2         |= UCA0TXIE;
                    UCA0TXBUF   = msg_CONTINUO[0];
                }
                else {
                    estado  = ESTADO_SIMPLE;
                    P1OUT   &= ~BIT0;

                    IE2         |= UCA0TXIE;
                    UCA0TXBUF   = msg_SIMPLE[0];
                }
            }
            else {                                                              // Sino solo permuto el led rojo y alta
                if (alta == ALTA) {
                    alta = BAJA;
                }
                else if (alta == BAJA) {
                    alta = ALTA;
                }

                P1OUT   ^= BIT6;
            }
            cont = 0;
        }
    }

    /************************
    *       PULSADOR S3     *
    *************************/

    else if ((P1IFG & BIT4) == BIT4) {

        cont        = 0;

        WDTCTL      = WDT_MDLY_32;                                              // Activo antirebote
        P1IE        &= ~(BIT3 | BIT4);
        P2IE        &= ~(BIT1 | BIT2 | BIT3);

        P1IFG       &= ~BIT4;                                                   // Limpio el flag y pongo tiempo = 1s
        tiempo      = UNS;

        if (alta == ALTA) {                                                     // Selecciono la nota correspondiente
            TA1CCR0    = DO5;
        }
        else if (alta == BAJA){
            TA1CCR0    = FA4;
        }

        if (estado == ESTADO_SIMPLE) {                                          // Si estoy en simple activo la interrupcion
            P2OUT       |= BIT4;                                                // del timer para detectar 1s antes de apagar
            TA1CTL      |= MC_1;                                                // los perifericos    

            TA0CTL      |= MC_1;

        }
        else if (estado == ESTADO_CONTINUO) {                                   // Si estoy en continuo, cambio el flanco a
                                                                                // Para saber cuando levanto el dedo y apagar 
            if ((P1IES & BIT4) == BIT4) {                                       // los perifericos
                P1IES   &= ~BIT4;

                P2OUT   |= BIT4;
                TA1CTL  |= MC_1;
            }

            else {
                P1IES   |= BIT4;

                P2OUT   &= ~BIT4;
                TA1CTL  &= ~MC_3;
            }
        }
    }

}
                                                                                /***********************
                                                                                *   MASCARA PUERTO 2   * 
                                                                                ************************/

#pragma vector = PORT2_VECTOR
__interrupt void RTI_PORT2 () {                                                 // Lo mismo de S3 con cada uno de los puertos

    cont    = 0;

    /************************
    *       PULSADOR S4     *
    *************************/

    if ((P2IFG & BIT1) == BIT1) {

        WDTCTL      = WDT_MDLY_32;
        P1IE        &= ~(BIT3 | BIT4);
        P2IE        &= ~(BIT1 | BIT2 | BIT3);

        P2IFG       &= ~BIT1;
        tiempo      = UNS;

        if (alta == ALTA) {
            TA1CCR0    = SI4;
        }
        else if (alta == BAJA) {
            TA1CCR0    = MI4;
        }

        if (estado == ESTADO_SIMPLE) {
            P2OUT       |= BIT5;
            TA1CTL      |= MC_1;

            TA0CTL      |= MC_1;

        }
        else if (estado == ESTADO_CONTINUO) {

            if ((P2IES & BIT1) == BIT1) {
                P2IES   &= ~BIT1;

                P2OUT   |= BIT5;
                TA1CTL  |= MC_1;
            }

            else {
                P2IES   |= BIT1;

                P2OUT   &= ~BIT5;
                TA1CTL  &= ~MC_3;
            }
        }
    }

    /************************
    *       PULSADOR S5     *
    *************************/

    else if ((P2IFG & BIT2) == BIT2) {

        WDTCTL      = WDT_MDLY_32;
        P1IE        &= ~(BIT3 | BIT4);
        P2IE        &= ~(BIT1 | BIT2 | BIT3);

        P2IFG       &= ~BIT2;
        tiempo      = UNS;

        if (alta == ALTA) {
            TA1CCR0    = LA4;
        }
        else if (alta == BAJA){
            TA1CCR0    = RE4;
        }

        if (estado == ESTADO_SIMPLE) {
            P2OUT       |= BIT6;
            TA1CTL      |= MC_1;

            TA0CTL      |= MC_1;

        }
        else if (estado == ESTADO_CONTINUO) {

            if ((P2IES & BIT2) == BIT2) {
                P2IES   &= ~BIT2;

                P2OUT   |= BIT6;
                TA1CTL  |= MC_1;
            }

            else {
                P2IES   |= BIT2;

                P2OUT   &= ~BIT6;
                TA1CTL  &= ~MC_3;
            }
        }
    }

    /************************
    *       PULSADOR S6     *
    *************************/

    else if ((P2IFG & BIT3) == BIT3) {

        WDTCTL      = WDT_MDLY_32;
        P1IE        &= ~(BIT3 | BIT4);
        P2IE        &= ~(BIT1 | BIT2 | BIT3);

        P2IFG       &= ~BIT3;
        tiempo      = UNS;

        if (alta == ALTA) {
            TA1CCR0    = SOL4;
        }
        else if (alta == BAJA){
            TA1CCR0    = DO4;
        }

        if (estado == ESTADO_SIMPLE) {
            P2OUT       |= BIT7;
            TA1CTL      |= MC_1;

            TA0CTL      |= MC_1;

        }
        else if (estado == ESTADO_CONTINUO) {

            if ((P2IES & BIT3) == BIT3) {
                P2IES   &= ~BIT3;

                P2OUT   |= BIT7;
                TA1CTL  |= MC_1;
            }

            else {
                P2IES   |= BIT3;

                P2OUT   &= ~BIT7;
                TA1CTL  &= ~MC_3;
            }
        }
    }

}
                                                                                /***********************
                                                                                *   MASCARA TIMER 0    * 
                                                                                ************************/

#pragma vector = TIMER0_A0_VECTOR
__interrupt void RTI_TIMER0() {

    TA0CCTL0        &= ~CCIFG;                                                  // Limpio el flag del timer
    TA0CTL          &= ~TAIFG;

    if (estado == ESTADO_SIMPLE || estado == ESTADO_CONTINUO) {                 // Si estoy en simple o remoto
        cont++;                                                                 // incremento el contador

        if (cont == tiempo) {                                                   // y compruebo si he alcanzado el tiempo
            TA0CTL      &= ~MC_3;                                               // que dependiendo del pulsador que presione 
            TA1CTL      &= ~MC_3;                                               // sera uno o dos segundos
            TA0R        = 0;

            P2OUT       &= ~(BIT4 | BIT5 | BIT6 | BIT7);                        // Cuando se alcanza el tiempo, apago todos
                                                                                // los leds y el zumbador,ademas de poner
            if (tiempo == UNS) {                                                // el timer en stop y limpiar TAR
                cont = 0;
            }
        }
    }

    else if (estado == ESTADO_REMOTO) {                                         // Si estoy en remoto debo permutar siempre el led 
        P1OUT       ^= BIT0;                                                    // verde para indicar el estado

        if (tiempo == UNS) {                                                    // y tras pasar un segundo, incremento contador
            cont ++;                                                            // siempre que tiempo sea 1s --> osea se haya 
            if (cont == tiempo) {                                               // presionado una tecla
                TA1CTL      &= ~MC_3;
                P2OUT       &= ~(BIT4 | BIT5 | BIT6 | BIT7);                    // si ha pasado el tiempo, apago los leds, el 
                cont = 0;                                                       // zumbador, pongo cont a 0 y tiempo a 2s para no
                tiempo = DOSS;                                                  // entrar de nuevo en esta zona
            }
        }
    }
}
                                                                                /***********************
                                                                                *   MASCARA WATCHDOG   * 
                                                                                ************************/
#pragma vector = WDT_VECTOR
__interrupt void RTI_WDT   () {
    P1IE        |= (BIT3 + BIT4);                                               // Reestablezco las interrupciones en los
    P2IE        |= (BIT1 + BIT2 + BIT3);                                        // pulsadores 

    IFG1        &= WDTIFG;                                                      // Desactivo Timer de watchdog
    WDTCTL      = (WDTPW + WDTHOLD);
}
                                                                                /******************************
                                                                                *   MASCARA UART TRANSMISION  * 
                                                                                *******************************/
#pragma vector = USCIAB0TX_VECTOR
__interrupt void RTI_UART_T() {                                                 // Si cambio de modo envio la cadena de 
    cont++;                                                                     // caracteres uno por uno incrementando cont
                                                                                // mientras que este sea menor a 3 y entonces
    if (estado == ESTADO_SIMPLE) {                                              // dessactivo las interrupciones de la UART 
        UCA0TXBUF   = msg_SIMPLE [cont];                                        // en modo transmision y cont = 0
    }

    else if (estado == ESTADO_CONTINUO) {
        UCA0TXBUF   = msg_CONTINUO [cont];
    }

    else if (estado == ESTADO_REMOTO) {
        UCA0TXBUF   = msg_REMOTO [cont];
    }

    if (cont == 3) {
        cont    = 0;
        IE2     &= ~UCA0TXIE;
    }
}
                                                                                /******************************
                                                                                *   MASCARA UART RECEPCION    * 
                                                                                *******************************/
#pragma vector = USCIAB0RX_VECTOR                   
__interrupt void RTI_UART_R() {                                                 // Tras recibir info por UART, compruebo
                                                                                // si estoy en estado remoto y luego 
    if (IFG2 & UCA0RXIFG) {                                                     // si es un 0 cambio de estado
        IFG2     &= ~UCA0RXIFG;                                                 // desactivando o activando las interrupciones
        cont     = 0;                                                           // de los distintos puertos y mandando la cadena
                                                                                // de caracteres correspondiente
        if (estado == ESTADO_REMOTO) {
            switch (UCA0RXBUF) {                                                // Compruebo el caracter recibido y segun 
                case '1': {                                                     // lo que le corresponda
                    tiempo  = UNS;                                              // establezco tiempo = 1s, TA1CCR0 le meto
                    TA1CCR0 = DO4;                                              // la frecuencia adecuada, enciendo el led
                    TA1CTL  |= MC_1;                                            // y el zumbador
                    P2OUT   |= BIT7;
                    break;
                }
                case '2': {
                    tiempo  = UNS;
                    TA1CCR0 = RE4;
                    TA1CTL  |= MC_1;
                    P2OUT   |= BIT6;
                    break;
                }
                case '3': {
                    tiempo  = UNS;
                    TA1CCR0 = MI4;
                    TA1CTL  |= MC_1;
                    P2OUT   |= BIT5;
                    break;
                }
                case '4': {
                    tiempo  = UNS;
                    TA1CCR0 = FA4;
                    TA1CTL  |= MC_1;
                    P2OUT   |= BIT4;
                    break;
                }
                case '5': {
                    tiempo  = UNS;
                    TA1CCR0 = SOL4;
                    TA1CTL  |= MC_1;
                    P2OUT   |= BIT7;
                    break;
                }
                case '6': {
                    tiempo  = UNS;
                    TA1CCR0 = LA4;
                    TA1CTL  |= MC_1;
                    P2OUT   |= BIT6;
                    break;
                }
                case '7': {
                    tiempo  = UNS;
                    TA1CCR0 = SI4;
                    TA1CTL  |= MC_1;
                    P2OUT   |= BIT5;
                    break;
                }
                case '8': {
                    tiempo  = UNS;
                    TA1CCR0 = DO5;
                    TA1CTL  |= MC_1;
                    P2OUT   |= BIT4;
                    break;
                }
            }
        }

        if (UCA0RXBUF == '0') {
            if (estado == ESTADO_REMOTO) {                                          // Cuando hay que cambiar de estado 
                estado = ESTADO_SIMPLE;                                             // Activo o desactivo las interrupciones de 
                                                                                    // los pulsadores, cont = 0,
                P1IE    |= (BIT3 | BIT4);                                           // pongo tiempo a 2s en remoto para
                P2IE    |= (BIT1 | BIT2 | BIT3);                                    // no entrar en zona de pulsador presionado
                                                                                    // dentro de la interrupcion del timer
                cont    = 0;
                P1OUT   &= ~BIT3;                                                   // enciendo el timer TIMER0_A0
                TA0CTL  &= ~MC_3;

                IE2     |= UCA0TXIE;                                                // envio el string
                UCA0TXBUF = msg_SIMPLE [0];
            }
            else if (estado == ESTADO_SIMPLE) {
                estado  = ESTADO_REMOTO;
                tiempo  = DOSS;
                P1IE    &= ~(BIT3 | BIT4);
                P2IE    &= ~(BIT1 | BIT2 | BIT3);

                TA0CTL  |= MC_1;

                IE2     |= UCA0TXIE;
                UCA0TXBUF = msg_CONTINUO [0];
            }
        }
    }
}
