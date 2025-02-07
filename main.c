/*
* Description: This is based on the open source firmware for TSDZ2 motor made by mbrusa
* It is adapted for TSDZ8 
*
* Related Document: See README.md
*
* Copyright (C) Casainho, Leon, MSpider65 2020.
*
* Released under the GPL License, Version 3
*/
#include "cybsp.h"
#include "cy_utils.h"

#include "cy_retarget_io.h"

#include "SEGGER_RTT.h"


#define MY_ENABLED 1
#define MY_DISABLED 0
#define uCPROBE_GUI_OSCILLOSCOPE MY_DISABLED // ENABLED
#if(uCPROBE_GUI_OSCILLOSCOPE == MY_ENABLED)
#include "ProbeScope/probe_scope.h"
#endif

#include "uart.h"
#include "motor.h"
#include "ebike_app.h"
#include "eeprom.h"


/* Define macro to enable/disable printing of debug messages */    // for debugging

#define ENABLE_XMC_DEBUG_VADC_PRINT       (0)
#define SEND_FRAME_100MS                  (0) // 1 = send the frame to the display (currently do not print them)
#define RECEIVE_FRAME                     (0) // 1 = receive frame from display
#define ENABLE_PRINT_SOME_DEBUG           (0)
/*******************************************************************************
* Macros
*******************************************************************************/
/* SysTick timer frequency in Hz  */
#define TICKS_PER_SECOND            1000 // 1 tick = 1msec


/*******************************************************************************
* Global Variables
*******************************************************************************/

/* Variable for keeping track of time */
volatile uint32_t system_ticks = 0;
uint32_t loop_25ms_ticks = 0;  
uint32_t start = 0 ; // mainly for debugging ; allow to print some variable every e.g. 5 sec

// maximum duty cycle
extern uint8_t ui8_pwm_duty_cycle_max; 

// for debugging only at the beginning
uint32_t count = 0;
uint32_t speed = 0; 
uint32_t pas_1 = 0;
uint32_t uart_rx = 0;
uint32_t brake = 0;
uint32_t unknown = 0;



//Used to store some data from interrupt and print in main loop at regular interval with
//               print_each_sec(uint32_t index, uint32_t value, uint32_t interval_ms)
uint32_t debug_values[20] ; // 
uint32_t debug_values_copy[20] = {0};
    

/* Declaration of array to store the message to be transmitted */
const uint8_t message[] = "Hello world!!\r\n";
const uint8_t clear_screen[] = "Hello from setup\r\n";


extern volatile uint8_t ui8_received_package_flag ;
extern volatile uint8_t ui8_tx_buffer[];

// for debugging // probably to remove todo
extern volatile uint32_t hall_print_pos ; 
extern volatile uint32_t hall_print_angle ;
extern volatile bool new_hall ;   
extern volatile uint32_t hall_print_pos2; // current hall pattern (after a sampling delay)
extern volatile uint32_t hall_print_interval ; // interval between 2 correct hall transitions
extern volatile uint32_t posif_SR0; 
extern volatile uint32_t posif_SR1;
extern volatile uint32_t posif_print_current_pattern ;

extern volatile uint32_t hall_pattern_irq;                   // current hall pattern
extern volatile uint16_t hall_pattern_change_ticks_irq; // ticks from ccu4 slice 2 for last pattern change
extern uint8_t  previous_hall_pattern; 

extern volatile uint16_t ui16_a ;
extern volatile uint16_t ui16_b ;
extern volatile uint16_t ui16_c ;
extern uint8_t ui8_svm_table_index_print  ; 
extern uint16_t ui16_temp_print  ;
extern uint16_t ui16_a_print ;
extern uint16_t ui16_new_angle_print; 
uint16_t ccu4_S2_timer ; 
extern uint16_t current_average ;
extern uint16_t hall_pattern_intervals[8];
// to measure time in irq
extern volatile uint16_t debug_time_ccu8_irq0; // to debug time in irq0 CCU8 (should be less than 25usec; 1 = 4 usec )
extern volatile uint16_t debug_time_ccu8_irq1; // to debug time in irq0 CCU8 (should be less than 25usec; 1 = 4 usec )
extern volatile uint16_t debug_time_ccu8_irq1b; // to debug time in irq0 CCU8 (should be less than 25usec; 1 = 4 usec )
extern volatile uint16_t debug_time_ccu8_irq1c; // to debug time in irq0 CCU8 (should be less than 25usec; 1 = 4 usec )
extern volatile uint16_t debug_time_ccu8_irq1d; // to debug time in irq0 CCU8 (should be less than 25usec; 1 = 4 usec )
extern volatile uint16_t debug_time_ccu8_irq1e; // to debug time in irq0 CCU8 (should be less than 25usec; 1 = 4 usec )

extern volatile uint8_t ui8_adc_battery_current_filtered;
extern uint8_t ui8_battery_current_filtered_x10;
extern uint16_t ui16_adc_battery_current_acc_X8;
extern uint16_t ui16_display_data_factor; 
extern uint8_t ui8_g_foc_angle;
extern uint8_t ui8_throttle_adc_in;

#if (PROCESS == FIND_BEST_GLOBAL_HALL_OFFSET)
extern uint16_t calibration_offset_current_average_to_display;
extern uint16_t calibration_offset_angle_to_display;
#endif
extern uint8_t global_offset_angle;
/*******************************************************************************
* Function Name: SysTick_Handler
********************************************************************************
* Summary:
* This is the interrupt handler function for the SysTick timer interrupt.
* It counts the time elapsed in milliseconds since the timer started. 
*******************************************************************************/
void SysTick_Handler(void)
{
    system_ticks++;
}

/*******************************************************************************
* Function Name: sys_now
********************************************************************************
* Summary:
* Returns the current time in milliseconds.
*
* Parameters:
*  none
*
* Return:
*  the current time in milliseconds
*
*******************************************************************************/
//__STATIC_INLINE uint32_t sys_now(void)
//{
//    return system_ticks;
//}

#define CHANNEL_NUMBER_PIN_2_2              (7U) // Torque
#define CHANNEL_NUMBER_PIN_2_3              (5U) // unknown
#define CHANNEL_NUMBER_PIN_2_4              (6U) // Battery
#define CHANNEL_NUMBER_PIN_2_5              (7U) // Throttle
#define CHANNEL_NUMBER_PIN_2_6              (0U) // Vcc
#define CHANNEL_NUMBER_PIN_2_7              (1U) // Unknown

#define CHANNEL_NUMBER_PIN_2_8              (1U) // Current 1 if group 0
#define CHANNEL_NUMBER_PIN_2_9              (2U) // Current U if group 0
#define CHANNEL_NUMBER_PIN_2_10             (3U) // Current v if group 0
#define CHANNEL_NUMBER_PIN_2_11             (4U) // Current W if group 0


// ************* declaration **********
void print_vadc(void);
void print_system_state();
void print_hall_pattern_debug();
void print_hall_pattern_debug2();
void print_motor_regulation_debug();

void jlink_print_system_state();

//my_CCU8_init(){
//}




/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
* This is the main function. It sets up a timer to trigger a 
* periodic interrupt. The main while loop checks for the elapsed time
* and toggles an LED at 1Hz to create an LED blinky. 
*
* Parameters:
*  none
*
* Return:
*  int
*
*******************************************************************************/

int main(void)
{
    cy_rslt_t result;

    uint32_t wait_time = 600000;
    while (wait_time > 0){  // wait a little at power on to let VCC be stable and so get probably better ADC conversions
        wait_time--;
    }
    /* Initialize the device and board peripherals */
    result = cybsp_init();
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    #if(uCPROBE_GUI_OSCILLOSCOPE == MY_ENABLED)
    ProbeScope_Init(20000);
    #endif
    // init segger to allow kind of printf
    SEGGER_RTT_Init ();     
    SEGGER_RTT_ConfigUpBuffer(0, NULL, NULL, 0, SEGGER_RTT_MODE_NO_BLOCK_TRIM);
    SEGGER_RTT_WriteString(0, RTT_CTRL_CLEAR); // clear the RTT terminal

    // set the PWM in such a way that PWM are set to passive levels when processor is halted for debugging (safety)
    XMC_CCU8_SetSuspendMode(ccu8_0_HW, XMC_CCU8_SUSPEND_MODE_SAFE_STOP);
    XMC_CCU4_SetSuspendMode(ccu4_0_HW, XMC_CCU4_SUSPEND_MODE_SAFE_STOP);

    /* Initialize printf retarget */
    cy_retarget_io_init(CYBSP_DEBUG_UART_HW);

    /* System timer configuration */
    SysTick_Config(SystemCoreClock / TICKS_PER_SECOND);
    // CCU8 slice 3 (IRQ at mid point) generates a SR3 when period match and this trigger a VADC group 0 for queue
    // CCU8 slice 2 (PWM) is configured in device generator to generate a sr2 when ONE match
    //  but device configurator does not allow to setup a second trigger for vadc queue conversion
    // some setup has to be done manually in the group1 queue
     // we have to connect sr2 to vadc group1 queue, to activate trigering and to disable gating.

    //Here I overwite the config defined in device manage (and generated in cycfg_peripherals.c)     
/*
 */
const XMC_VADC_QUEUE_CONFIG_t vadc_0_group_0_queue_config2 =
{
    .conv_start_mode = (uint32_t) XMC_VADC_STARTMODE_WFS,
    .req_src_priority = (uint32_t) XMC_VADC_GROUP_RS_PRIORITY_3,
    .src_specific_result_reg = (uint32_t) 0,
    .trigger_signal = (uint32_t) XMC_VADC_REQ_TR_P, // use gate set up
    .trigger_edge = (uint32_t) XMC_VADC_TRIGGER_EDGE_ANY,
    .gate_signal = (uint32_t) XMC_VADC_REQ_GT_E, //use CCU8_ST3A = when timer is at mid period counting up
    .timer_mode = (uint32_t) false,
    .external_trigger = (uint32_t) true,
};
    XMC_VADC_GROUP_QueueSetGatingMode(vadc_0_group_0_HW, (XMC_VADC_GATEMODE_t) XMC_VADC_GATEMODE_IGNORE);
    XMC_VADC_GROUP_QueueInit(vadc_0_group_0_HW, &vadc_0_group_0_queue_config2);

const XMC_VADC_QUEUE_CONFIG_t vadc_0_group_1_queue_config2 = {
        .conv_start_mode = (uint32_t) XMC_VADC_STARTMODE_WFS,
        .req_src_priority = (uint32_t) XMC_VADC_GROUP_RS_PRIORITY_2,
        .src_specific_result_reg = (uint32_t) 0,
        .trigger_signal = (uint32_t) XMC_VADC_REQ_TR_P,  // use gate set up
        .trigger_edge = (uint32_t) XMC_VADC_TRIGGER_EDGE_ANY,
        .gate_signal = (uint32_t) XMC_VADC_REQ_GT_E, ////use CCU8_ST3A = when timer is at mid period counting up
        .timer_mode = (uint32_t) false,
        .external_trigger = (uint32_t) true,
    };
    XMC_VADC_GROUP_QueueSetGatingMode(vadc_0_group_1_HW, (XMC_VADC_GATEMODE_t) XMC_VADC_GATEMODE_IGNORE);
    XMC_VADC_GROUP_QueueInit(vadc_0_group_1_HW, &vadc_0_group_1_queue_config2);

/*  
    #define CCU8_SR2_GxREQTRI 8 // SR2 from CCU8 slice 2
    const XMC_VADC_QUEUE_CONFIG_t vadc_0_group_1_queue_config2 = {
        .conv_start_mode = (uint32_t) XMC_VADC_STARTMODE_WFS,
        .req_src_priority = (uint32_t) XMC_VADC_GROUP_RS_PRIORITY_3,
        .src_specific_result_reg = (uint32_t) 0,
        .trigger_signal = (uint32_t) CCU8_SR2_GxREQTRI,
        .trigger_edge = (uint32_t) XMC_VADC_TRIGGER_EDGE_RISING,
        .gate_signal = (uint32_t) 0X01, //DISCARD_VADC_GATING
        .timer_mode = (uint32_t) false,
        .external_trigger = (uint32_t) true,
    };
    XMC_VADC_GROUP_QueueSetGatingMode(vadc_0_group_1_HW, (XMC_VADC_GATEMODE_t) XMC_VADC_GATEMODE_IGNORE);
    XMC_VADC_GROUP_QueueInit(vadc_0_group_1_HW, &vadc_0_group_1_queue_config2);
*/    


    /*Generate a load event to start background request source continuous conversion*/
    //XMC_VADC_GLOBAL_BackgroundTriggerConversion(VADC);

    // to test : this could be replaced by an interrupt on CCU4 slice 1 SR0 ( capture done)
    //NVIC_SetPriority(POSIF0_0_IRQn, 1U); //Testing with SR 0 when POSIF detect a valid transition (for debug)
	//NVIC_EnableIRQ(POSIF0_0_IRQn);
    
    // use CCU4 SR0 (slice 1 external event from POSIT) to say that a correct event occured
    //NVIC_SetPriority(CCU40_0_IRQn, 1U); //Testing with SR 0 when POSIF detect a valid transition (for debug)
	//NVIC_EnableIRQ(CCU40_0_IRQn);
    NVIC_SetPriority(CCU40_1_IRQn, 0U); //capture hall pattern and slice 2 time when a hall change occurs
	NVIC_EnableIRQ(CCU40_1_IRQn);
    

    /* CCU80_0_IRQn and CCU80_1_IRQn. slice 3 interrupt on counting up and down. at 19 khz to manage rotating flux*/
	NVIC_SetPriority(CCU80_0_IRQn, 1U);
	NVIC_EnableIRQ(CCU80_0_IRQn);
    NVIC_SetPriority(CCU80_1_IRQn, 1U);
	NVIC_EnableIRQ(CCU80_1_IRQn);

    // set initial position of hall sensor and first next expected one in shadow and load immediately in real register
    //posif_init_position();
    get_hall_pattern();
    previous_hall_pattern = 0; // use a different hall pattern to force the angle. 
    XMC_POSIF_Start(HALL_POSIF_HW);
    
    /* Enable Global Start Control CCU80  in a synchronized way*/
    XMC_SCU_SetCcuTriggerHigh(SCU_GENERAL_CCUCON_GSC80_Msk);
    XMC_SCU_SetCcuTriggerLow(SCU_GENERAL_CCUCON_GSC80_Msk);
    uint32_t retry_start_counter = 10;
    while ((!XMC_CCU8_SLICE_IsTimerRunning(PHASE_U_TIMER_HW)) && (retry_start_counter > 0)){ // to be sure it is running
        XMC_SCU_SetCcuTriggerHigh(SCU_GENERAL_CCUCON_GSC80_Msk);
        XMC_SCU_SetCcuTriggerLow(SCU_GENERAL_CCUCON_GSC80_Msk);
    
        //SEGGER_RTT_printf(0, "Retry CCU8 start; still %u try\r\n", retry_counter);
    }

//    XMC_VADC_GLOBAL_EnablePostCalibration(vadc_0_HW, 0U);
//    XMC_VADC_GLOBAL_EnablePostCalibration(vadc_0_HW, 1U);
//    XMC_VADC_GLOBAL_StartupCalibration(vadc_0_HW);
    XMC_WDT_Service();
    // todo should be adapted to get them from flash memory; currently we only use default)
    init_extra_fields_config (); // get the user parameters (
    // todo : change when eeprom is coded properly add some initialisation (e.g. m_configuration_init() and ebike_app.init)
    // currently it is filled with parameters from user setup + some dummy values (e.g. for soc)
    m_configuration_init();
    
    // add some initialisation in ebike_app.init
    #if (PROCESS != DETECT_HALL_SENSORS_POSITIONS ) // is not done when we are just testing slow motion to detect hall pattern
    XMC_WDT_Service();
    ebike_app_init();
    #else
    XMC_WDT_Stop();  // do not use watchdog when running this part of the code
    log_hall_sensor_position();  // let the motor run slowly (10 turns) in each direction, log via jlink the angles of hall pattern changes
    // note: this function never ends
    #endif
    start = system_ticks;

    
//***************************** while ************************************
    while (1) // main loop
    {     
	    // avoid a reset
        XMC_WDT_Service(); // reset if we do not run here within the 0,5 sec
    
        // when there is no frame waiting for process (ui8_received_package_flag == 0), try to get incoming data from UART but do not process them
        // When frame is full, data are processed in ebike_app.c once every 100 msec
        if (ui8_received_package_flag == 0) {
            fillRxBuffer();
            // if (ui8_received_package_flag) SEGGER_RTT_printf(0,"Frame in\r\n"); // just for debug
        }
        // to be activated for real production
        // Here we should call a funtion every 25 msec (based on systick or on an interrupt based on a CCU4 timer)
        #if (PROCESS != DETECT_HALL_SENSORS_POSITIONS )
        if ((system_ticks - loop_25ms_ticks) > 25) { 
            loop_25ms_ticks = system_ticks;
            ebike_app_controller();  // this performs some checks and update some variable every 25 msec
        }
        #endif
              
        #if (uCPROBE_GUI_OSCILLOSCOPE == MY_ENABLED)
        ProbeScope_Sampling(); // this should be moved e.g. in a interrupt that run faster
        #endif
        
        // for debug
    #if (DEBUG_ON_JLINK == 1)
         // do debug if communication with display is working
        //if( take_action(1, 250)) SEGGER_RTT_printf(0,"Light is= %u\r\n", (unsigned int) ui8_lights_button_flag);
        if (ui8_system_state) { // print a message when there is an error detected
            if( take_action(4,500)) jlink_print_system_state();
        }
//        if( take_action(2, 500)) SEGGER_RTT_printf(0,"Adc current= %u adcX8=%u  current_Ax10=%u  factor=%u\r\n", 
//            (unsigned int) ui8_adc_battery_current_filtered ,
//            (unsigned int) ui16_adc_battery_current_acc_X8 ,
//            (unsigned int) ui8_battery_current_filtered_x10 , 
//            (unsigned int) ui16_display_data_factor
//            );
        // monitor duty cycle, current when motor is running
        /*
        if( take_action(3, 1000)) SEGGER_RTT_printf(0,"dctarg=%u dc=%u    ctarg=%u cfilt=%u Throttle=%u  erps=%u foc%u\r\n",
            (unsigned int) ui8_controller_duty_cycle_target,
            (unsigned int) ui8_g_duty_cycle,
            (unsigned int) ui8_controller_adc_battery_current_target,
            (unsigned int) ui8_adc_battery_current_filtered,
            (unsigned int) ui8_throttle_adc_in,
            (unsigned int) ui16_motor_speed_erps,
            (unsigned int) ui8_g_foc_angle
            //(unsigned int) XMC_CCU8_SLICE_IsTimerRunning(PHASE_U_TIMER_HW),
            //(unsigned int) ui8_motor_enabled
        );
        */
        // monitor results of find best global hall offset
        #if ( PROCESS == FIND_BEST_GLOBAL_HALL_OFFSET ) 
        if( take_action(4, 500)) SEGGER_RTT_printf(0,"offset=%u current=%u erps=%u foc%u   dctarg=%u dc=%u    ctarg=%u cfilt=%u\r\n",
            (unsigned int) calibration_offset_angle_to_display ,
            (unsigned int) calibration_offset_current_average_to_display,
            (unsigned int) ui16_motor_speed_erps,
            (unsigned int) ui8_g_foc_angle,
            (unsigned int) ui8_controller_duty_cycle_target,
            (unsigned int) ui8_g_duty_cycle,
            (unsigned int) ui8_controller_adc_battery_current_target,
            (unsigned int) ui8_adc_battery_current_filtered
        );
        #endif
        #if ( PROCESS == TEST_WITH_FIXED_DUTY_CYCLE ) || ( PROCESS == TEST_WITH_THROTTLE )
        if( take_action(4, 500)) SEGGER_RTT_printf(0,"trotl=%u erps=%u foc%u   dctarg=%u dc=%u    ctarg=%u cfilt=%u\r\n",
            (unsigned int) ui8_throttle_adc_in,
            (unsigned int) ui16_motor_speed_erps,
            (unsigned int) ui8_g_foc_angle,
            (unsigned int) ui8_controller_duty_cycle_target,
            (unsigned int) ui8_g_duty_cycle,
            (unsigned int) ui8_controller_adc_battery_current_target,
            (unsigned int) ui8_adc_battery_current_filtered
        );    
        #endif
    #endif    
    #if (DEBUG_ON_UART == 1 )
        if (new_hall) {
            ccu4_S2_timer = XMC_CCU4_SLICE_GetTimerValue(RUNNING_250KH_TIMER_HW);
            //SEGGER_RTT_printf(0,"angle= %u pins= %u a=%u b=%u c=%u tim=%u\r\n ",
            //    (uint32_t) ui16_new_angle_print , (uint32_t) hall_print_pos , (uint32_t) ui16_a , (uint32_t)ui16_b , (uint32_t)ui16_c ,
            //     (uint32_t)ccu4_S2_timer);
            printf("angle= %u pins= %u a=%u b=%u c=%u tim=%u\r\n ",
                 (unsigned int) ui16_new_angle_print , (unsigned int) hall_print_pos ,  (unsigned int) ui16_a , (unsigned int) ui16_b , (unsigned int) ui16_c ,
                 (unsigned int) ccu4_S2_timer);
            new_hall = false;
        }
        
        //to debug print any error
        if (ui8_system_state) { // print a message when there is an error detected
                if( take_action(4,500)) print_system_state();
        }
        #if (ENABLE_PRINT_SOME_DEBUG == 1)
        //if (take_action(0, 250)) print_hall_pattern_debug();
        //if (take_action(6, 250)) print_motor_regulation_debug(); // print duty cycle and current (target ar real)
        
        // print Throttle  throttle gr1 ch7 result 5  in bg  p2.5)
        //if (take_action(3,500)) printf("Throttle adc 8 bits=%u\r\n", (XMC_VADC_GROUP_GetResult(vadc_0_group_1_HW , 5 ) & 0x0FFF) >> 4); 
       
       // print torque sensor group 0 ch7 , result 2 , pin 2.2
       if (take_action(7,500)) printf("Torque adc 12 bits=%u\r\n", (XMC_VADC_GROUP_GetResult(vadc_0_group_0_HW , 2 ) & 0xFFFF) );  

       // print batery sensor group 1 ch6 , result 4 , pin 2.4
       //if (take_action(8,500)) printf("                      Battery adc 12 bits=%u\r\n", (XMC_VADC_GROUP_GetResult(vadc_0_group_1_HW , 4 ) & 0xFFFF) );  
        if (take_action(8,500)) printf("                      Battery mv=%u\r\n",ui16_battery_voltage_filtered_x1000);
       // print CCU4 torque is running or not
       //if (take_action(9,500)) printf("CCU4 s3= %u\r\n", XMC_CCU4_SLICE_GetTimerValue(PWM_TORQUE_TIMER_HW  ) );  

       //print_hall_pattern_debug2(); // print first 100 passage in ccu8 irq0
        
       #endif
    #endif   
       
    } // end while main loop
}


void jlink_print_system_state(){
    switch (ui8_system_state) {
        case 1: 
            SEGGER_RTT_printf(0,"Error : overvoltage\r\n");
            break;
        case 2: 
            SEGGER_RTT_printf(0,"Error : torque_sensor\r\n");
            break;
        case 3: 
            SEGGER_RTT_printf(0,"Error : cadence_sensor\r\n");
            break;
        case 4: 
            SEGGER_RTT_printf(0,"Error : motor_blocked\n");
            break;
        case 5: 
            SEGGER_RTT_printf(0,"Error : throttle\r\n");
            break;
        case 6: 
            SEGGER_RTT_printf(0,"Error : overtemperature\r\n");
            break;
        case 7: 
            SEGGER_RTT_printf(0,"Error : battery_overcurrent\r\n");
            break;
        case 8: 
            SEGGER_RTT_printf(0,"Error : speed_sensor\r\n");
            break;
        case 9: 
            SEGGER_RTT_printf(0,"Error : motor_check\r\n");
            break;
             
    }
}    

void print_system_state(){
    switch (ui8_system_state) {
        case 1: 
            printf("Error : overvoltage\r\n");
            break;
        case 2: 
            printf("Error : torque_sensor\r\n");
            break;
        case 3: 
            printf("Error : cadence_sensor\r\n");
            break;
        case 4: 
            printf("Error : motor_blocked\n");
            break;
        case 5: 
            printf("Error : throttle\r\n");
            break;
        case 6: 
            printf("Error : overtemperature\r\n");
            break;
        case 7: 
            printf("Error : battery_overcurrent\r\n");
            break;
        case 8: 
            printf("Error : speed_sensor\r\n");
            break;
        case 9: 
            printf("Error : motor_check\r\n");
            break;
             
    }
}    

void print_motor_regulation_debug(){
        debug_values_copy[11] = debug_values[11] ;// ui8_controller_duty_cycle_target;
        debug_values_copy[1] = debug_values[1] ;// ui8_g_duty_cycle;
        debug_values_copy[12] = debug_values[12] ;// ui8_controller_adc_battery_current_target;
        debug_values_copy[3] = debug_values[3] ; // ui8_adc_battery_current_filtered;
        debug_values_copy[14] =debug_values[14]; // ui8_adc_battery_current_acc
        debug_values_copy[15] = (XMC_VADC_GROUP_GetResult(vadc_0_group_0_HW , 8 ) & 0xFFFF);
        debug_values_copy[16] = (XMC_VADC_GROUP_GetResult(vadc_0_group_1_HW , 12 ) & 0xFFFF);
        printf("dctarg=%u dc=%u  ctarg=%u cfilt=%u acc=%u G0=%u G1=%u\r\n",
            (unsigned int) debug_values_copy[11], (unsigned int) debug_values_copy[1],(unsigned int) debug_values_copy[12],
             (unsigned int) debug_values_copy[3],(unsigned int) debug_values_copy[14],
            (unsigned int) debug_values_copy[15],
            (unsigned int) debug_values_copy[16]);
    
}
void print_hall_pattern_debug(){
        debug_values_copy[0] = debug_values[0] ; // ui8_svm_table_index
        debug_values_copy[1] = debug_values[1] ; // ui8_g_duty_cycle;
        debug_values_copy[2] = debug_values[2] ; // ui8_controller_adc_battery_current_target;
        debug_values_copy[3] = debug_values[3] ; // ui8_adc_battery_current_filtered;
        debug_values_copy[4] = debug_values[4] ; // ui8_motor_commutation_type;
        debug_values_copy[5] = debug_values[5] ; // ui8_interpolation_angle ;
        debug_values_copy[6] = debug_values[6] ; // enlapsed_time tick;
        debug_values_copy[7] = debug_values[7] ; // compensated_enlapsed_time;
        debug_values_copy[8] = debug_values[8] ; // ui16_hall_counter_total; 
        debug_values_copy[9] = debug_values[9] ; // hall_pattern_error_counter;
        debug_values_copy[10] = debug_values[10] ; // hall_pattern_valid_counter;
        debug_values_copy[11] = debug_values[11] ;// ui8_controller_duty_cycle_target;
        debug_values_copy[13]  = debug_values[13] ; //global_offset_angle;
        uint32_t current_adc = (uint32_t) (XMC_VADC_GROUP_GetResult(vadc_0_group_1_HW , 12 ) & 0xFFFF); // current adc
        // current_average (calculated based on the 2 VADC on pin 2.8) for the same value of hall debug offset 
        // 6 ticks intervals between hall patern changes
        SEGGER_RTT_WriteString(0, "SEGGER Real-Time-Terminal Sample\r\n\r\n");
        printf("idx=%u dc=%u ct=%u cf=%u cty=%u ia=%u et=%u ct=%0u hct=%u hpe=%u hpv=%u dct=%u doa=%u cadc=%u cavg=%u, 1=%u 2=%u 3=%u 4=%u  5=%u  6=%u\r\n",
            (unsigned int) debug_values_copy[0], (unsigned int) debug_values_copy[1],(unsigned int) debug_values_copy[2],
            (unsigned int) debug_values_copy[3], (unsigned int) debug_values_copy[4] , (unsigned int) debug_values_copy[5],
            (unsigned int) debug_values_copy[6], (unsigned int) debug_values_copy[7], (unsigned int) debug_values_copy[8],
            (unsigned int) debug_values_copy[9], (unsigned int) debug_values_copy[10] ,(unsigned int) debug_values_copy[11] ,
            (unsigned int)  debug_values_copy[13] , (unsigned int) current_adc, current_average,
            (unsigned int) hall_pattern_intervals[1],(unsigned int) hall_pattern_intervals[2],(unsigned int) hall_pattern_intervals[3],
            (unsigned int) hall_pattern_intervals[4],(unsigned int) hall_pattern_intervals[5],(unsigned int) hall_pattern_intervals[6] );
        
        /*
        SEGGER_RTT_printf(0, "idx=%-3u dc=%-3u ct=%-4u cf=%-4u cty=%-4u ia=%-6u et=%-5u ct=%-10u hct=%-5u hpe=%-3u hpv=%-3u dct=%-3u doa=%-3u cadc=%-4u cavg=%-4u, 1=%-4u 2=%-4u 3=%-4u 4=%-4u  5=%-4u  6=%-4u\r\n",
            debug_values_copy[0], debug_values_copy[1],debug_values_copy[2],
            debug_values_copy[3], debug_values_copy[4] , debug_values_copy[5],
            debug_values_copy[6], debug_values_copy[7], debug_values_copy[8],
            debug_values_copy[9], debug_values_copy[10] ,debug_values_copy[11] , debug_values_copy[13] , current_adc, current_average,
            hall_pattern_intervals[1],hall_pattern_intervals[2],hall_pattern_intervals[3],
            hall_pattern_intervals[4],hall_pattern_intervals[5],hall_pattern_intervals[6] );
        */    
        // %[flags][FieldWidth][.Precision]ConversionSpecifier%
        // c	one char , d signed integer, u unsigned integer , x	hexadecimal integer , s	string , p  pointer?  8-digit hexadecimal integer. (Argument shall be a pointer to void.)
        printf("in irq0=%uus irq1=%uus irq1b=%uus irq1c=%uus irq1d=%uus irq1e=%uus\r\n", 
            debug_time_ccu8_irq0 << 2,debug_time_ccu8_irq1 << 2,debug_time_ccu8_irq1b << 2,
            debug_time_ccu8_irq1c << 2,debug_time_ccu8_irq1d << 2,debug_time_ccu8_irq1e << 2);
        debug_time_ccu8_irq0 = 0;
        debug_time_ccu8_irq1 = 0;
        debug_time_ccu8_irq1b = 0;
        debug_time_ccu8_irq1c = 0;
        debug_time_ccu8_irq1d = 0;
        debug_time_ccu8_irq1e = 0;
        #ifdef XMC_ASSERT_ENABLE
        printf("XMC_ASSERT_ENABLE\n\r");
        #endif
    
}

uint32_t first_debug_values[100][20];
uint32_t first_debug_index = 0; 
#define LINES 98
void print_hall_pattern_debug2(){
    
    if (first_debug_index == LINES){
        printf("Now since pattern angle delay d_c_t d_c cur_t curr      error valid com_t rot_ticks ofa refV\r\n");
        for (uint32_t i = 0; i<LINES; i++){
            for (uint32_t j = 0; j<15; j++){
                printf("%u ", (unsigned int) first_debug_values[i][j]);
                if (j == 8) {    // put a space before the error counter
                    printf("    ");
                }
            }
            printf("\r\n");
        }
    }
}

/* [] END OF FILE */
