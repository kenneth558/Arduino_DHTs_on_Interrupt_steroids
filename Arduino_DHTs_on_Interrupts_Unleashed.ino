/*                      So, what does this sketch do?  
 *  This sketch obtains accurate and thorough PCINT data by toggling every digital pin.  Circuit faults that prevent pins from toggling their 
 *  voltage levels will prevent accurate assessment by this sketch, if such pins are served by the PCINT infrastructure.  Original header files are 
 *  still referred to for port mask data, however. Boards tested with: UNO Nano Leonardo Mega2560 WeMo XI/TTGO XI (8F328P-U)
Note that with the WeMo XI/TTGO XI board you'll get useful info if you'll just forgive the ending getting abbreviated.
Thank you
 */

#ifndef NOT_AN_INTERRUPT //This macro was introduced on Oct 1, 2013, IDE version 1.5.5-r2
Did you use your system package manager to install an obsolete Arduino IDE rather than downloading the current IDE directly from arduino.cc?
/*
This section is here for one purpose - to trigger a compile-time error if you are compiling with an obsolete Arduino IDE,
If the line above causes a compile-time error, there are two possible reasons listed below.  Reason #1 below is what we are trying to catch.

 1 )  You are using an obsolete IDE.  This will be the case, for example, if you installed your IDE via a Linux repository rather than from arduino.cc
     If above reason #1 is the case, do yourself a favor and install a newer Arduino IDE directly from arduino.cc instead of using your system's package manager.
     See https://github.com/arduino/Arduino/releases/latest
 
 2 )  Your board truly does not have features to necessitate this by design, or you are compiling in a mixed-technology environment.
     If above reason #2 is the case, simply edit this sketch ( comment out or remove the invalid instruction line/section ), then save and recompile it to get rid of the compile-time error.  With your board, you forfeit the ability to use resistors for mildly helpful boot-up options.  Not a big deal at all.
*/
#endif
#ifndef u8
    #define u8 uint8_t
#endif
#ifndef u16
    #define u16 uint16_t
#endif
#ifndef __LGT8FX8E__
    short unsigned _baud_rate_ = 57600;//Very much dependent upon the capability of the host computer to process talkback data, not just baud rate of its interface
#else
    short unsigned _baud_rate_ = 19200;//In production environment the XI tends to power up at baud 19200 so we can't risk setting baud to anything but that
    #define LED_BUILTIN 12
#endif
#include "analog_pin_adjust.h"
// You as end-user can specify in next line the pins that you want protected from the signals placed on ISR( PCINT_vect )-capable pins during the DHT discovery process.  These pins will not be mucked with, but nor will they then support an ISR( PCINTn_vect )-serviced DHT device.
const u8 pins_NOT_safe_even_to_make_low_Z_during_testing[ ] = { };
#if defined ( SERIAL_PORT_HARDWARE ) && defined ( LED_BUILTIN )
    const u8 pins_NOT_safe_to_toggle_during_testing[ ] = { SERIAL_PORT_HARDWARE, LED_BUILTIN };
#else
    #ifdef LED_BUILTIN
        const PROGMEM u8 pins_NOT_safe_to_toggle_during_testing[ ] = { LED_BUILTIN };
    #else
        const PROGMEM u8 pins_NOT_safe_to_toggle_during_testing[ ] = { };
    #endif
#endif
/*
 * If the line above causes a compile fail, you'll need to adjust it per your needs or upgrade your compiler/IDE
 */
#include "misc_maskportitems.h"
const u8 dht_max_transitions_for_valid_acquisition_stream = 42;
#include "structs.h"
ISRXREF* Isrxref;
//PINXREF* Pinxref;
PORTXREF* Portxref;
ISRSPEC* Isrspec;
PORTSPEC* Portspec;
DEVSPEC* Devspec;

//bool mswindows = false;  //Used for line-end on serial outputs.  Will be determined true during run time if a 1 Megohm ( value not at all critical as long as it is large enough ohms to not affect operation otherwise ) resistor is connected from pin LED_BUILTIN to PIN_A0
u8 number_of_ports_found = 0; //Doesn't ever need to be calculated a second time, so make global and calculate in setup
u8 number_of_devices_found = 0;
u8 number_of_populated_isrs = 0;
const PROGMEM unsigned long halftime = ( ( unsigned long ) -1 )>>1;
const PROGMEM u8 allowed_number_consecutive_read_failures = 25;//JUST A GUESS, NOT EVEN EMPIRICS TO SUPPORT THIS
const PROGMEM u8 consecutive_reads_to_verify_device_type = 20;
const PROGMEM u8 best_uSec_time_translate = 100;
const PROGMEM u8 alert_beyond_this_number_of_consecutive_errs = 5;
u8 number_of_ports_with_functioning_DHT_devices_and_serviced_by_ISR = 0; //Needs to be calculated every time discovery of DHT devices function is executed
static port_specific* ptr_to_portspecs_stack;
static port_specific* previous_ptr_to_portspecs_stack = NULL;
static u8 numOfPortsWithAnyDHTDevice;
char* ports_string_in_heap_array;
char* pre_array_boards_ports_string;
byte* ISR_WITH_DHT_port_pinmask_stack_array;
byte* PCMSK_indexwise_array;//One byte per ISR, values change each time PCMSK for that ISR's PCINT changes 
byte* DHT_without_ISR_port_pinmask_stack_array;
byte* previous_ISR_WITH_DHT_port_pinmask_stack_array = NULL;
u8 number_of_elements_in_ISR_part_of_port_pinmask_stack = 0;
#ifndef PCINT_B_vect
    #ifdef PCMSK
    u8 number_of_elements_in_PCMSK_port_pinmask_stack_array = 0;//0-8 This relates to PCMSK of ISR, one element per PCMSK bit serving a DHT
    byte* PCMSK_port_pinmask_stack_array;//Each bit in PCMSK3 starting with FIRST USED bit, ending with last USED bit.  This schema allows for memory space savings
    #endif
    #ifdef PCMSK0
    u8 number_of_elements_in_PCMSK_port_pinmask_stack_array0 = 0;//0-8 This relates to PCMSK0 ( ISR0 ), one element per PCMSK bit serving a DHT
    byte* PCMSK0_port_pinmask_stack_array;//Each bit in PCMSK0 starting with FIRST USED bit, ending with last USED bit.  This schema allows for memory space savings
    #endif
    #ifdef PCMSK1
    u8 number_of_elements_in_PCMSK_port_pinmask_stack_array1 = 0;//0-8 This relates to PCMSK1 ( ISR1 ), one element per PCMSK bit serving a DHT
    byte* PCMSK1_port_pinmask_stack_array;//Each bit in PCMSK1 starting with FIRST USED bit, ending with last USED bit.  This schema allows for memory space savings
    #endif
    #ifdef PCMSK2
    u8 number_of_elements_in_PCMSK_port_pinmask_stack_array2 = 0;//0-8 This relates to PCMSK2 ( ISR2 ), one element per PCMSK bit serving a DHT
    byte* PCMSK2_port_pinmask_stack_array;//Each bit in PCMSK2 starting with FIRST USED bit, ending with last USED bit.  This schema allows for memory space savings
    #endif
    #ifdef PCMSK3
    u8 number_of_elements_in_PCMSK_port_pinmask_stack_array3 = 0;//0-8 This relates to PCMSK3 ( ISR3 ), one element per PCMSK bit serving a DHT
    byte* PCMSK3_port_pinmask_stack_array;//Each bit in PCMSK3 starting with FIRST USED bit, ending with last USED bit.  This schema allows for memory space savings
    #endif
#else
    This sketch does not work on the ATtiny4313 right now
#endif
u8 number_of_elements_in_non_ISR_DHT_ports_stack = 0;

const u8 element_bytes_in_ISR_part_of_port_pinmask = 5;//
const u8 element_bytes_in_DHT_part_of_port_pinmask = 3;
const u8 element_bytes_in_PCMSK_part_of_port_pinmask = 4;//

const u8 millis_DHT_MCU_start_signal_bit[ ] = { 19, 2 };//consider the first one lost due to resolution err and System millis are a little short anyway, so adding one for margin. NOTE - the manufacturer's latest data sheets indicate a trend to reduce the MCU start bit lengths into the uSecs.

const struct one_line_device_protocols_supported DEV_PROT_DHT11 = {
     millis_DHT_MCU_start_signal_bit[ 0 ], 10000, 5000, dht_max_transitions_for_valid_acquisition_stream //Note that the manufacturer recommends 5 second wait intervals between reads in a continuous reading environment such as this
};

const struct one_line_device_protocols_supported DEV_PROT_DHT22 = {
    millis_DHT_MCU_start_signal_bit[ 0 ], 10000, 5000, dht_max_transitions_for_valid_acquisition_stream 
};

volatile DEVICE_PROTOCOL Devprot[ ] = { DEV_PROT_DHT11, DEV_PROT_DHT22 };// DHT11 first, DHT22 second.  Since it is by value, we can change values as more suitable params are determined via tests even though the original copies are consts

#include "ISRs.h"

int freeRam () 
{ 
  extern int __heap_start, *__brkval; 
  int v; 
  return ( int ) &v - ( __brkval == 0 ? ( int ) &__heap_start : ( int ) __brkval ); 
 }
    

void prep_ports_for_detection()
{
//We malloc here instead of using the stack so we can run strchr on the string
    pre_array_boards_ports_string = ( char* )malloc( 1 + ( as_many_ports_as_a_board_could_ever_have * 2 ) ); //Temporary array. This will be the absolute maximum number of ports that could ever need to be examined for DHT devices on them.
    if ( !pre_array_boards_ports_string )
    {
/* */
        Serial.begin( _baud_rate_ ); //This speed is very dependent on the host's ability
        Serial.setTimeout( 10 ); //
        while ( !Serial ) { 
          ; // wait for serial port to connect. Needed for Leonardo's native USB
        }
/* */
        Serial.print( F( "Not able to acquire enough heap memory to properly prepare any DHT device to respond in detection process." ) );
        Serial.println();
        Serial.flush();
        Serial.end();
/* */   delay( 10000 );
        return;//TODO: handle a return from here with some kind of propriety
    }
    pre_array_boards_ports_string[ 0 ] = 0;
    delay( 5 );//to allow devices to settle down before making their pins into outputs HIGH
    byte eligible_devices_this_port = 0;//If we did this global, it would take a byte
    number_of_ports_found = 0;
//For every pin, check the ports_found to see if the return value is already in there.  If not in the array, add it to the end of the array.  string is best type
    for ( u8 pin = 0; pin < NUM_DIGITAL_PINS; pin++ ) //purpose for this is purely to determine number of devices connected to ports/pins to size the local-scope array
    {
        if( !pin_in_protected_arrays( pin ) )
        { 
            char portchar = ( char ) ( digitalPinToPort( pin ) + 64 );                                                      //Compute the alpha of the port
            if ( strchr( pre_array_boards_ports_string, portchar ) == NULL )                                                       //if it is not already found in the array of supporting ports found in pinset
            {
                eligible_devices_this_port = 0;
                pre_array_boards_ports_string[ number_of_ports_found ] = portchar;
                pre_array_boards_ports_string[ ++number_of_ports_found ] = 0;
                for( u8 tmp_pin = pin; tmp_pin < NUM_DIGITAL_PINS; tmp_pin++ )//for remainder of pins
                {
                    if( ( digitalPinToPort( pin ) == digitalPinToPort( tmp_pin ) ) && !pin_in_protected_arrays( tmp_pin ) && tmp_pin != LED_BUILTIN )
                    {
                        eligible_devices_this_port |= digitalPinToBitMask( tmp_pin );
                    }
                }
                *portModeRegister( digitalPinToPort( pin ) ) = eligible_devices_this_port;
                *portOutputRegister( digitalPinToPort( pin ) ) = eligible_devices_this_port;
                pre_array_boards_ports_string[ number_of_ports_found + as_many_ports_as_a_board_could_ever_have ] = eligible_devices_this_port;//by using number_of_ports_found after it got incremented, we continue to allow for zero termed string at beginning
            }
        }
    }
    delay( 2000 );//to allow devices to settle down before expecting DHTs to act right
}

bool build_from_nothing()
{
    u8 duplicate_pin_higher = 0;
    u8 duplicate_pin_lower = 0;
    if ( F_CPU != 16000000 )
    { //In here we change the two checktimes for other clock rates TODO
/* */
        Serial.begin( _baud_rate_ ); //This speed is very dependent on the host's ability
        Serial.setTimeout( 10 ); //
        while ( !Serial ) { 
          ; // wait for serial port to connect. Needed for Leonardo's native USB
        }
/* */
        Serial.print( F( "Not working with an expected clock rate, so DHT devices will not be detected." ) );
        Serial.println();
        Serial.flush();
        Serial.end();
/* */   delay( 10000 );
/* */
    }
    if( !( bool )number_of_ISRs )
    { //In here we change the two checktimes for other clock rates TODO
/* */
        Serial.begin( _baud_rate_ ); //This speed is very dependent on the host's ability
        Serial.setTimeout( 10 ); //
        while ( !Serial ) { 
          ; // wait for serial port to connect. Needed for Leonardo's native USB
        }
/* */
        Serial.print( F( "No ISR was found in this microcontroller board, so this product won't perform optimally.  Your very next step should be to check if any PCMSK variables are used with this board, or check connections and reboot if this message might reflect some fault condition..." ) );
        Serial.println();
        Serial.flush();
        Serial.end();
/* */   delay( 10000 );
    }
    prep_ports_for_detection(); 
//Make a way to keep the info in the array so to avoid having to obtain it all over again
    free ( pre_array_boards_ports_string ); //Just as a matter of good practice if we want to copy-paste this section, doesn't really do anything here
    pre_array_boards_ports_string = ( char* )malloc( as_many_ports_as_a_board_could_ever_have ); //Temporary array. This will be the absolute maximum number of ports that could ever need to be examined for DHT devices on them.
    previous_ISR_WITH_DHT_port_pinmask_stack_array = ( byte* )pre_array_boards_ports_string;//everywhere else, this line will go prior to the free instruction above, and it will better match the names of the pointers
    number_of_ports_found = 0;
    number_of_devices_found = 0;
    pre_array_boards_ports_string[ 0 ] = 0;
    byte eligible_devices_this_port = 0;
    u8 pre_array_devspec_index[ NUM_DIGITAL_PINS ];//
    u8 populated_port_count = 0;//
    char string_of_all_ports_that_are_populated[ as_many_ports_as_a_board_could_ever_have ] = { 0 };//

//For every pin, check the ports_found to see if the return value is already in there.  If not in the array, add it to the end of the array.  string is best type
    for ( u8 pin = 0; pin < NUM_DIGITAL_PINS; pin++ ) 
    {
        pre_array_devspec_index[ pin ] = 0;
    }
    for ( u8 pin = 0; pin < NUM_DIGITAL_PINS; pin++ ) //purpose for this is purely to determine number of ports connected to pins to size the local scope array
    {
        if( !pin_in_protected_arrays( pin ) )
        { 
            char portchar = ( char ) ( digitalPinToPort( pin ) + 64 );                                                      //Compute the alpha of the port
            if ( strchr( pre_array_boards_ports_string, portchar ) == NULL )                                                       //if it is not already found in the array of supporting ports found in pinset
            {
                eligible_devices_this_port = 0;
                pre_array_boards_ports_string[ number_of_ports_found ] = portchar;
                pre_array_boards_ports_string[ ++number_of_ports_found ] = 0;
                for( u8 tmp_pin = pin; tmp_pin < NUM_DIGITAL_PINS; tmp_pin++ )//for remainder of pins
                {
                    if( !pin_in_protected_arrays( tmp_pin ) && tmp_pin != LED_BUILTIN )
                        { 
                            eligible_devices_this_port |= digitalPinToBitMask( tmp_pin );
                        }
                }
                eligible_devices_this_port = find_all_dhts_this_port( digitalPinToPort( pin ), eligible_devices_this_port );
                if( eligible_devices_this_port )
                {
                    string_of_all_ports_that_are_populated[ populated_port_count ] = portchar;
                    string_of_all_ports_that_are_populated[ ++populated_port_count ] = 0;
                    for( u8 tmp_pin = pin; tmp_pin < NUM_DIGITAL_PINS; tmp_pin++ )//for remainder of pins again, see all on this port that need a devspec entry
                    {
                        if ( digitalPinToPort( pin ) == digitalPinToPort( tmp_pin ) )
                            if ( eligible_devices_this_port & digitalPinToBitMask( tmp_pin ) )
                            {
                                pre_array_devspec_index[ number_of_devices_found++ ] = tmp_pin;//TODO: see if we need to add one to pin number//building a sparsed array
                                eligible_devices_this_port &= ~digitalPinToBitMask( tmp_pin );//This will make sure that duplicate pins with higher digital pin numbers get ignored
                            }
                    }
                }
            }
        }
    }
//At this point we have an array called pre_array_devspec_index holding sparsed pin numbers of devices
// we know the number of elements and stored that number in number_of_devices_found
// array called string_of_all_ports_that_are_populated holding sparsed port letters of device-populated ports
// so we know by that array size - 1 how many ports have devices on them
    free ( pre_array_boards_ports_string );//purpose for this heap-scope var was purely to determine number of ports connected to pins to size the setup()-scope array
    previous_ISR_WITH_DHT_port_pinmask_stack_array = ( byte* )pre_array_boards_ports_string;//Just as a matter of good practice, doesn't really do anything here. everywhere else before a free instruction, this line will better match the names of the pointers
    free ( previous_ISR_WITH_DHT_port_pinmask_stack_array ); //Just as a matter of good practice, doesn't really do anything here
unsigned long int main_array_size_now = sizeof( ISRXREF ) + \
        sizeof( PORTXREF ) + \
        ( sizeof( ISRSPEC ) * number_of_ISRs ) + \
        ( sizeof( PORTSPEC ) * populated_port_count ) + \
        ( sizeof( DEVSPEC ) * number_of_devices_found ) + \
         strlen( string_of_all_ports_that_are_populated ) + \
         1;//string_of_all_ports_that_are_populated
        free( ( void* )Isrxref );
    Isrxref = ( ISRXREF* )malloc( main_array_size_now );
    if( !Isrxref )
    { //Come up with a better way to handle this
        Serial.begin( _baud_rate_ ); //This speed is very dependent on the host's ability
        Serial.setTimeout( 10 ); //
        while ( !Serial ) { 
          ; // wait for serial port to connect. Needed for Leonardo's native USB
        }
        Serial.print( F( "Memory space of " ) );
        Serial.print( main_array_size_now );
        Serial.print( F( " bytes was refused allocation.  Aborting..." ) );
        Serial.println();
        Serial.flush();
        Serial.end();
        return ( false );

    }
    if( previous_ISR_WITH_DHT_port_pinmask_stack_array != NULL && previous_ISR_WITH_DHT_port_pinmask_stack_array != ( byte* )Isrxref )//  Entered state of possibility of memory fragmentation
        if ( previous_ISR_WITH_DHT_port_pinmask_stack_array < ( byte* )Isrxref ) mem_frag_alert();
        else mem_defrag_alert();
    number_of_elements_in_ISR_part_of_port_pinmask_stack = number_of_ports_found;//This is just a temporary number until detection and re-allocate the array
    previous_ISR_WITH_DHT_port_pinmask_stack_array = ( byte* )Isrxref;
    Portxref = ( PORTXREF* )( ( long unsigned int )Isrxref + sizeof( ISRXREF ) );
    Isrspec = ( ISRSPEC* )( ( long unsigned int )Portxref + sizeof( PORTXREF ) );
    Portspec = ( PORTSPEC* )( ( long unsigned int )Isrspec + ( sizeof( ISRSPEC) * number_of_ISRs ) );
    Devspec = ( DEVSPEC* )( ( long unsigned int )Portspec + ( sizeof( PORTSPEC ) * populated_port_count ) );
    ports_string_in_heap_array = ( char* )( ( long unsigned int )Devspec + ( sizeof( DEVSPEC ) * number_of_devices_found ) );
    strcpy( ports_string_in_heap_array, string_of_all_ports_that_are_populated );//This makes ports_string_in_heap_array not suitable for interrupt findings if any pins served by interrupts don't have devices on them!!!
    ports_string_in_heap_array[ strlen( ports_string_in_heap_array ) + 1 ] = 255;//This now makes a two-byte marker: 0, 255 to help determine if heap gets overwritten by stack (ISRs and local vars)
    for ( u8 i = 0; i < number_of_ISRs; i++ )
    {
        Isrxref->ISR_xref[ i ] = i;
        Isrxref->my_isrspec_addr[ i ] = &Isrspec[ i ];//Will only work if and because Isrspec has not been sparsed, yet
    }
delay( 5 );//This is to let all dht devices that got triggered to end their data streams
    for( u8 i = 0; i < number_of_devices_found; i++ )//make sure we never make the number of elements in devspec_index a different amount than this line thinks
    {
        digitalWrite( pre_array_devspec_index[ i ], HIGH );
        Devspec[ i ].Dpin = pre_array_devspec_index[ i ];
        for( u8 ij = 0; ij < sizeof( Devspec[ i ].last_valid_data_bytes_from_dht_device )/ sizeof( Devspec[ i ].last_valid_data_bytes_from_dht_device[ 0 ] ); ij++ )
            Devspec[ i ].last_valid_data_bytes_from_dht_device[ ij ] = 0;
        Devspec[ i ].timestamp_of_pin_valid_data_millis = 0;
        Devspec[ i ].devprot_index = 0;
        Devspec[ i ].consecutive_read_failures_mode0 = 0;
        Devspec[ i ].consecutive_read_failures_mode1 = 0;
        Devspec[ i ].consecutive_read_failures_mode2 = 0;
        Devspec[ i ].consecutive_read_failures_mode3 = 0;
        Devspec[ i ].consecutive_read_failures_mode4 = 0;
        Devspec[ i ].consecutive_read_successes = 0;
        Devspec[ i ].start_time_plus_max_acq_time_in_uSecs = 0;
        long unsigned timenow = millis();//A single point of reference to prevent changing during the following
        Devspec[ i ].device_busy_resting_this_more_millis = Devprot[ Devspec[ i ].devprot_index ].millis_rest_length;
        if( !Devspec[ i ].device_busy_resting_this_more_millis )
        {
          Devspec[ i ].device_busy_resting_this_more_millis++; //zero is not a valid value unless device is rested
        }
        Devspec[ i ].mask_in_port = digitalPinToBitMask( pre_array_devspec_index[ i ] );
        Devspec[ i ].output_port_reg_addr = portOutputRegister( digitalPinToPort( Devspec[ i ].Dpin ) );
        Devspec[ i ].ddr_port_reg_addr = portModeRegister( digitalPinToPort( Devspec[ i ].Dpin ) );
        Devspec[ i ].pin_reg_addr = portInputRegister( digitalPinToPort( Devspec[ i ].Dpin ) );
        Devspec[ i ].index_of_next_valid_readings_sets = 0;
    }
delay( 2000 );//ensure all devices get a rest period right here
    for( u8 fill_index_in_heap_proarray = 0; fill_index_in_heap_proarray < populated_port_count; fill_index_in_heap_proarray++ )
    {
        Portspec[ fill_index_in_heap_proarray ].this_port_index_in_order_of_discovery_traversing_pins = fill_index_in_heap_proarray;//not necessary?
        Portspec[ fill_index_in_heap_proarray ].this_port_index_from_core_lookup_alphabetic_order = string_of_all_ports_that_are_populated[ fill_index_in_heap_proarray ] - 64;
        Portspec[ fill_index_in_heap_proarray ].this_port_main_reg = ( u8* )portOutputRegister( string_of_all_ports_that_are_populated[ fill_index_in_heap_proarray ] - 64 );
        Portspec[ fill_index_in_heap_proarray ].this_port_pin_reg = ( u8* )portInputRegister( string_of_all_ports_that_are_populated[ fill_index_in_heap_proarray ] - 64 );
        Portspec[ fill_index_in_heap_proarray ].this_port_mode_reg = ( u8* )portModeRegister( string_of_all_ports_that_are_populated[ fill_index_in_heap_proarray ] - 64 );
        Portspec[ fill_index_in_heap_proarray ].mask_of_safe_pins_to_detect_on = 0;
        Portspec[ fill_index_in_heap_proarray ].mask_of_unsafe_pins_even_to_make_low_Z = 0;
        for( u8 prot_pin_index = 0; prot_pin_index < sizeof( pins_NOT_safe_even_to_make_low_Z_during_testing ); prot_pin_index++ )
            if( digitalPinToPort( pins_NOT_safe_even_to_make_low_Z_during_testing[ prot_pin_index ] ) == Portspec[ fill_index_in_heap_proarray ].this_port_index_from_core_lookup_alphabetic_order ) Portspec[ fill_index_in_heap_proarray ].mask_of_unsafe_pins_even_to_make_low_Z |= digitalPinToBitMask( pins_NOT_safe_even_to_make_low_Z_during_testing[ prot_pin_index ] );
        Portspec[ fill_index_in_heap_proarray ].mask_of_unsafe_pins_to_toggle_and_otherwise = 0;
        for( u8 prot_pin_index = 0; prot_pin_index < sizeof( pins_NOT_safe_to_toggle_during_testing ); prot_pin_index++ )
            if( digitalPinToPort( pins_NOT_safe_to_toggle_during_testing[ prot_pin_index ] ) == Portspec[ fill_index_in_heap_proarray ].this_port_index_from_core_lookup_alphabetic_order ) Portspec[ fill_index_in_heap_proarray ].mask_of_unsafe_pins_to_toggle_and_otherwise |= digitalPinToBitMask( pins_NOT_safe_to_toggle_during_testing[ prot_pin_index ] );
        Portspec[ fill_index_in_heap_proarray ].mask_of_real_pins_this_port = 0;
        for( u8 _pin_ = 0; _pin_ < NUM_DIGITAL_PINS; _pin_++ )
            if( digitalPinToPort( _pin_ ) == Portspec[ fill_index_in_heap_proarray ].this_port_index_from_core_lookup_alphabetic_order ) Portspec[ fill_index_in_heap_proarray ].mask_of_real_pins_this_port |= digitalPinToBitMask( _pin_ );
        Portspec[ fill_index_in_heap_proarray ].mask_of_DHT_devices_this_port = 0;
        Portspec[ fill_index_in_heap_proarray ].PCINT_pins_mask = 0;
        Portspec[ fill_index_in_heap_proarray ].timestamp_of_last_portwide_device_detection_action_by_thisport_micros = micros();
        

        u8 l = 0;
        for( ; l < number_of_devices_found; l++ )
        {
            if( Portspec[ fill_index_in_heap_proarray ].this_port_index_from_core_lookup_alphabetic_order == digitalPinToPort( Devspec[ l ].Dpin ) )
            {
            }
        }
    }
    for( u8 i = 0; i < sizeof( Portxref->PORT_xref ); i++ )
        Portxref->PORT_xref[ i ] = i;
    Serial.begin( _baud_rate_ ); //This speed is very dependent on the host's ability
    Serial.setTimeout( 10 ); //
    while ( !Serial ) { 
      ; // wait for serial port to connect. Needed for Leonardo's native USB
    }
    Serial.println();
    Serial.println();
    Serial.print( F( "LIST OF " ) );
    Serial.print( NUM_DIGITAL_PINS );
    Serial.print( F( " DIGITAL PINS." ) );
#ifdef NUM_ANALOG_INPUTS
    Serial.print( F( "  Note there are " ) );
    Serial.print( NUM_ANALOG_INPUTS );
    Serial.print( F( " analog input pins, only the ones that can be made digital are shown here alongside their digital numbers" ) );
#endif
    Serial.println();
    for ( u8 pinxref_index = 0; pinxref_index < NUM_DIGITAL_PINS; pinxref_index++ )
    {
        Serial.print( F( "Pin D" ) );
        Serial.print( pinxref_index );
        if ( pinxref_index < 10 ) Serial.print( F( "  " ) );
        else Serial.print( F( " " ) );
        print_analog_if_exists( pinxref_index );
        Serial.print( F( ":" ) );
        char portchar = ( char ) ( digitalPinToPort( pinxref_index ) + 64 );                                                      //Compute the alpha of the port
        Serial.print( F( "PORT" ) );
        Serial.print( portchar );
        Serial.print( F( " " ) );
        Serial.print( F( "bit " ) );
        u8 _bit = digitalPinToBitMask( pinxref_index );
        u8 counter = 0;
        for( ; _bit >>= 1; counter++ );
        Serial.print( counter );
        for( u8 i = 0; i < number_of_devices_found; i++ )//make sure we never make the number of elements in devspec_index a different amount than this line thinks
        {
            if( Devspec[ i ].Dpin == pinxref_index ) Serial.print( F( " --DHT connected--" ) );
        }
        if( pin_in_protected_arrays( pinxref_index ) )
        { 
            Serial.print( F( " pin is protected by it being listed in a protected pins array" ) );
        }
        
        if ( pinxref_index == LED_BUILTIN )
        { 
            Serial.print( F( ", LED_BUILTIN " ) );//compiler ( one version in Linux Mint, at least ) is so problematic with printing the word "builtin" ( either case ) as the last thing on the line that we can't do it straightforwardly, so the space is added to end.  Simply amazing...
        }
        Serial.println();
        if( !duplicate_pin_higher )
        {
            for( u8 dup_pin_index = pinxref_index + 1; dup_pin_index < NUM_DIGITAL_PINS; dup_pin_index++ )
            {
                if( digitalPinToPort( dup_pin_index ) == digitalPinToPort( pinxref_index ) && digitalPinToBitMask( dup_pin_index ) == digitalPinToBitMask( pinxref_index ) )
                {
                    duplicate_pin_higher = dup_pin_index;
                    duplicate_pin_lower = pinxref_index;
                    break;
                }
            }
        }
    }
    if( duplicate_pin_higher )
    {
        Serial.print( F( "Note: starting with digital pin number " ) );
        Serial.print( duplicate_pin_higher );
        Serial.print( F( " that is a duplicate of digital pin number " ) );
        Serial.print( duplicate_pin_lower );
        Serial.print( F( ", one" ) );
        Serial.println();
        Serial.print( F( "or more of these pins are virtual pins that are duplicates of real ones that have a lower" ) );
        Serial.println();
        Serial.print( F( "number, in which case a device connected to such will only show as being connected the" ) );
        Serial.println();
        Serial.print( F( "lower numbered pin.  Due to general Arduino memory space limitations, this sketch only lets" ) );
        Serial.println();
        Serial.print( F( "you know of the first one." ) );
        Serial.println();
    }
    for ( u8 devspec_index = 0; \
    devspec_index < number_of_devices_found; \
    devspec_index++ )
    //loop while within number of elements of pinspec AND the next element will not be null
    //purpose for this is to make the different pin masks for each array port element, and make the Pinspec, Devspec, Devprot array elements
    {
        char portchar = ( char ) ( digitalPinToPort( Devspec[ devspec_index ].Dpin ) + 64 );                                                      //Compute the alpha of the port
        if ( strchr( ports_string_in_heap_array, portchar ) != NULL )                                                      //if it is found in the array of supporting ports found in pinset
        {
            u8 str_index = ( u8 ) ( strchr( ports_string_in_heap_array, portchar ) - ports_string_in_heap_array );//
            if( Portspec[ str_index ].this_port_index_from_core_lookup_alphabetic_order == digitalPinToPort( Devspec[ devspec_index ].Dpin ) )//does this port belong to this pin
            { //OF COURSE IT WILL MATCH.  TODO:  DELETE THIS CHECK or keep as only of these two checks
                if( !( pin_in_protected_arrays( Devspec[ devspec_index ].Dpin ) || Devspec[ devspec_index ].Dpin == LED_BUILTIN ) ) //testing-eligible:
                { 
                    Portspec[ str_index ].mask_of_safe_pins_to_detect_on |= digitalPinToBitMask( Devspec[ devspec_index ].Dpin );
                }
            }
        }
    }
    delay( 5 );//to allow devices to settle down before making their pins into outputs HIGH    
    for ( u8 port_placement = 0; port_placement < ( ( unsigned long )Devspec - ( unsigned long )Portspec )/ sizeof( PORTSPEC ); port_placement++ )
    {
        *( Portspec[ port_placement ].this_port_mode_reg ) = Portspec[ port_placement ].mask_of_safe_pins_to_detect_on;//Make safe and eligible pins into outputs
        *( Portspec[ port_placement ].this_port_main_reg ) = Portspec[ port_placement ].mask_of_safe_pins_to_detect_on; // set output pins HIGH for 2 seconds that are still eligible
    }
    delay( 2000 );//some types of device needed their line high like this for their wait period, so we do it for entire system
    unsigned long time_this_port_tested_millis[ ( ( unsigned long )Devspec - ( unsigned long )Portspec )/ sizeof( PORTSPEC ) ];
    for ( u8 port_placement = 0; port_placement < ( ( unsigned long )Devspec - ( unsigned long )Portspec )/ sizeof( PORTSPEC ); port_placement++ )
    {//find dht devices each port, number of ports = ( ( unsigned long )Pinspec - ( unsigned long )Portspec )/ sizeof( PORTSPEC )
        eligible_devices_this_port = Portspec[ port_placement ].mask_of_safe_pins_to_detect_on;
        eligible_devices_this_port = find_all_dhts_this_port( Portspec[ port_placement ].this_port_index_from_core_lookup_alphabetic_order, eligible_devices_this_port );//determikne if we really should send port_placement 
        if ( ( bool ) ( eligible_devices_this_port ) )
        { //DO NOT SPARSE THE ARRAY YET BECAUSE THESE PORT ENTRIES NEED TO EXIST FOR THE ISR DISCOVERY PROCESS LATER.  IF THEY DON'T EXIST THEN, WE WOULD NEED MORE ELABORATE CODING THERE TO ACCOMODATE NON-EXISTENT PORT ENTRIES
            Portspec[ port_placement ].mask_of_DHT_devices_this_port = eligible_devices_this_port;//keep this out of the previous function called so that function can be used prior to this var existing
            time_this_port_tested_millis[ port_placement ] = millis();//don't yet know the particular rest period each device needs, but we are working towards guaranteeing them their rest after this exhausting acquisition
        }
    }
    Serial.begin( _baud_rate_ ); //This speed is very dependent on the host's ability
    Serial.setTimeout( 10 ); //
    while ( !Serial ) { 
      ; // wait for serial port to connect. Needed for Leonardo's native USB
    }
    Serial.println();
    Serial.print( F( "DHT devices found on pins:" ) );//
    Serial.println();
    u8 DHT_count = 0;
        for( u8 devspec_index = 0; devspec_index < ( ( long unsigned int )ports_string_in_heap_array - ( long unsigned int )Devspec ) / sizeof( DEVSPEC ); devspec_index++ )
        {
            ++DHT_count;
            Serial.print( F( "    D" ) );
            Serial.print( Devspec[ devspec_index ].Dpin );
            if ( Devspec[ devspec_index ].Dpin < 10 ) Serial.print( F( "  " ) );
            else Serial.print( F( " " ) );
            print_analog_if_exists( Devspec[ devspec_index ].Dpin );
            Serial.println(); 
        }
    if( !DHT_count )
        Serial.print( F( "No " ) );
    else
    {
        Serial.print( F( "Total of " ) );
        Serial.print( DHT_count );
    }
    Serial.print( F( " DHT devices are connected" ) );
    Serial.println();

    Serial.println();
    Serial.flush();
    Serial.end();
}

void print_analog_if_exists( u8 pin )
{ 
    bool this_pin_is_also_analog = false; //will this line suffice?
    this_pin_is_also_analog = false;
#ifdef PIN_A0
        if( digitalPinToPort( pin ) == digitalPinToPort( PIN_A0 ) && digitalPinToBitMask( pin ) == digitalPinToBitMask( PIN_A0 ) ) //Reason we can't just compare pin to definition is because that will miss several real pins considering that analog pins map to duplicate virtual ones if available instead of real ones
        { 
            this_pin_is_also_analog = true;
            Serial.print( F( "(A0) " ) );
        }
#endif
#ifdef PIN_A1
        if( digitalPinToPort( pin ) == digitalPinToPort( PIN_A1 ) && digitalPinToBitMask( pin ) == digitalPinToBitMask( PIN_A1 ) )
        { 
            this_pin_is_also_analog = true;
            Serial.print( F( "(A1) " ) );
        }
#endif
#ifdef PIN_A2
        if( digitalPinToPort( pin ) == digitalPinToPort( PIN_A2 ) && digitalPinToBitMask( pin ) == digitalPinToBitMask( PIN_A2 ) )
        { 
            this_pin_is_also_analog = true;
            Serial.print( F( "(A2) " ) );
        }
#endif
#ifdef PIN_A3
        if( digitalPinToPort( pin ) == digitalPinToPort( PIN_A3 ) && digitalPinToBitMask( pin ) == digitalPinToBitMask( PIN_A3 ) )
        { 
            this_pin_is_also_analog = true;
            Serial.print( F( "(A3) " ) );
        }
#endif
#ifdef PIN_A4
        if( digitalPinToPort( pin ) == digitalPinToPort( PIN_A4 ) && digitalPinToBitMask( pin ) == digitalPinToBitMask( PIN_A4 ) )
        { 
            this_pin_is_also_analog = true;
            Serial.print( F( "(A4) " ) );
        }
#endif
#ifdef PIN_A5
        if( digitalPinToPort( pin ) == digitalPinToPort( PIN_A5 ) && digitalPinToBitMask( pin ) == digitalPinToBitMask( PIN_A5 ) )
        { 
            this_pin_is_also_analog = true;
            Serial.print( F( "(A5) " ) );
        }
#endif
#ifdef PIN_A6
        if( digitalPinToPort( pin ) == digitalPinToPort( PIN_A6 ) && digitalPinToBitMask( pin ) == digitalPinToBitMask( PIN_A6 ) )
        { 
            this_pin_is_also_analog = true;
            Serial.print( F( "(A6) " ) );
        }
#endif
#ifdef PIN_A7
        if( digitalPinToPort( pin ) == digitalPinToPort( PIN_A7 ) && digitalPinToBitMask( pin ) == digitalPinToBitMask( PIN_A7 ) )
        { 
            this_pin_is_also_analog = true;
            Serial.print( F( "(A7) " ) );
        }
#endif
#ifdef PIN_A8
        if( digitalPinToPort( pin ) == digitalPinToPort( PIN_A8 ) && digitalPinToBitMask( pin ) == digitalPinToBitMask( PIN_A8 ) )
        { 
            this_pin_is_also_analog = true;
            Serial.print( F( "(A8) " ) );
        }
#endif
#ifdef PIN_A9
        if( digitalPinToPort( pin ) == digitalPinToPort( PIN_A9 ) && digitalPinToBitMask( pin ) == digitalPinToBitMask( PIN_A9 ) )
        { 
            this_pin_is_also_analog = true;
            Serial.print( F( "(A9) " ) );
        }
#endif
#ifdef PIN_A10
        if( digitalPinToPort( pin ) == digitalPinToPort( PIN_A10 ) && digitalPinToBitMask( pin ) == digitalPinToBitMask( PIN_A10 ) )
        { 
            this_pin_is_also_analog = true;
            Serial.print( F( "(A10)" ) );
        }
#endif
#ifdef PIN_A11
        if( digitalPinToPort( pin ) == digitalPinToPort( PIN_A11 ) && digitalPinToBitMask( pin ) == digitalPinToBitMask( PIN_A11 ) )
        { 
            this_pin_is_also_analog = true;
            Serial.print( F( "(A11)" ) );
        }
#endif
#ifdef PIN_A12
        if( digitalPinToPort( pin ) == digitalPinToPort( PIN_A12 ) && digitalPinToBitMask( pin ) == digitalPinToBitMask( PIN_A12 ) )
        { 
            this_pin_is_also_analog = true;
            Serial.print( F( "(A12)" ) );
        }
#endif
#ifdef PIN_A13
        if( digitalPinToPort( pin ) == digitalPinToPort( PIN_A13 ) && digitalPinToBitMask( pin ) == digitalPinToBitMask( PIN_A13 ) )
        { 
            this_pin_is_also_analog = true;
            Serial.print( F( "(A13)" ) );
        }
#endif
#ifdef PIN_A14
        if( digitalPinToPort( pin ) == digitalPinToPort( PIN_A14 ) && digitalPinToBitMask( pin ) == digitalPinToBitMask( PIN_A14 ) )
        { 
            this_pin_is_also_analog = true;
            Serial.print( F( "(A14)" ) );
        }
#endif
#ifdef PIN_A15
        if( digitalPinToPort( pin ) == digitalPinToPort( PIN_A15 ) && digitalPinToBitMask( pin ) == digitalPinToBitMask( PIN_A15 ) )
        { 
            this_pin_is_also_analog = true;
            Serial.print( F( "(A15)" ) );
        }
#endif
//No boards known to have more than 16 analog pins, but that knowledge might be inaccurate or reality could change....
#ifdef PIN_A16
        if( digitalPinToPort( pin ) == digitalPinToPort( PIN_A16 ) && digitalPinToBitMask( pin ) == digitalPinToBitMask( PIN_A16 ) )
        { 
            this_pin_is_also_analog = true;
            Serial.print( F( "(A16)" ) );
        }
#endif
#ifdef PIN_A17
        if( digitalPinToPort( pin ) == digitalPinToPort( PIN_A17 ) && digitalPinToBitMask( pin ) == digitalPinToBitMask( PIN_A17 ) )
        { 
            this_pin_is_also_analog = true;
            Serial.print( F( "(A17)" ) );
        }
#endif
#ifdef PIN_A18
        if( digitalPinToPort( pin ) == digitalPinToPort( PIN_A18 ) && digitalPinToBitMask( pin ) == digitalPinToBitMask( PIN_A18 ) )
        { 
            this_pin_is_also_analog = true;
            Serial.print( F( "(A18)" ) );
        }
#endif
#ifdef PIN_A19
        if( digitalPinToPort( pin ) == digitalPinToPort( PIN_A19 ) && digitalPinToBitMask( pin ) == digitalPinToBitMask( PIN_A19 ) )
        { 
            this_pin_is_also_analog = true;
            Serial.print( F( "(A19)" ) );
        }
#endif
#ifdef PIN_A20
        if( digitalPinToPort( pin ) == digitalPinToPort( PIN_A20 ) && digitalPinToBitMask( pin ) == digitalPinToBitMask( PIN_A20 ) )
        { 
            this_pin_is_also_analog = true;
            Serial.print( F( "(A20)" ) );
        }
#endif
        if( !this_pin_is_also_analog ) Serial.print( F( "     " ) );
 }

void mem_frag_alert()
{ 
    Serial.begin( _baud_rate_ ); //This speed is very dependent on the host's ability
    Serial.setTimeout( 10 ); //
    while ( !Serial ) { 
      ; // wait for serial port to connect. Needed for Leonardo's native USB
    }

#ifndef __LGT8FX8E__
    Serial.print( F( "Alert: Possible memory fragmentation as evidenced by internal acquisition of a memory block in a higher address of heap memory while changing the size of an internal array." ) );
    Serial.println();
    Serial.print( F( "Memory fragmentation is NOT a fault condition - it is simply non-ideal due to its long term effects, and the developers of this product have gone to lengths to prevent it from happening on their account.  Accruing memory fragmentation long term usually eventually leads to unpredictable/degraded/unstable/locked operation.  Advisable action if this is a mission-critical application and this message appears periodically: reboot this device at your very next opportunity" ) );
#else
    Serial.print( F( "Possible memory fragmentation" ) );
#endif
    Serial.println();
    Serial.flush();
    Serial.end();
 }

void mem_defrag_alert()
{ 
    Serial.begin( _baud_rate_ ); //This speed is very dependent on the host's ability
    Serial.setTimeout( 10 ); //
    while ( !Serial ) { 
      ; // wait for serial port to connect. Needed for Leonardo's native USB
    }
#ifndef __LGT8FX8E__
    Serial.print( F( "Improved memory efficiency just occurred as evidenced by internal acquisition of a memory block in a lower address of heap memory while changing the size of an internal array.  Fragmentation has decreased." ) );
#else
    Serial.print( F( "Fragmentation improved." ) );
#endif
    Serial.println();
    Serial.flush();
    Serial.end();

 }

bool pin_NOT_safe_even_to_make_low_Z_during_testing( u8 pin )
{ 
    for ( u8 f = 0; f < sizeof( pins_NOT_safe_even_to_make_low_Z_during_testing ); f++ )
    { 
        if ( pin == pins_NOT_safe_even_to_make_low_Z_during_testing[ f ] )
        { 
            return ( true );//Portspecmask_of_safe_pins_to_detect_on
        }
    }
 }

bool pin_in_protected_arrays( u8 pin )
{ 
    for ( u8 f = 0; f < sizeof( pins_NOT_safe_even_to_make_low_Z_during_testing ); f++ )
    { 
        if ( pin == pins_NOT_safe_even_to_make_low_Z_during_testing[ f ] )
        { 
            return ( true );
        }
    }
    for ( u8 f = 0; f < sizeof( pins_NOT_safe_to_toggle_during_testing ); f++ )
    { 
        if ( pin == pins_NOT_safe_to_toggle_during_testing[ f ] )
        { 
            return ( true );
        }
    }
    return ( false );
 }

byte find_all_dhts_this_port( u8 port_index, u8 eligible_devices_this_port ) //Would be good also to discover each device's type but that would mean returning an array, an identifier byte for each port bit: 8 bytes return value?
{
    byte* port_ = ( byte* )portOutputRegister( port_index );
    byte* pinreg_ = ( byte* )portInputRegister( port_index );
    byte* portreg_ = ( byte* )portModeRegister( port_index );
    u8 index = 0; //This here so later we can make it part of the array, if necessary
    unsigned long mid_bit_start_bit_low_checktime = 35;
    unsigned long mid_bit_start_bit_high_checktime = 120;
    unsigned long turnover_reference_time;
    u8 i;
    //save the state of this port, pin directions and pullup or not and state ( if output ), restore when leaving
    byte startstate_port = *port_;// & valid_pins_this_port; //The AND boolean operation is done to mask out bits that don't have pins for them
    byte startstate_portreg = *portreg_;// & valid_pins_this_port;
    byte startstate_pinreg = *pinreg_;// & valid_pins_this_port;
    if ( !( bool ) eligible_devices_this_port ) goto this_port_done_for_dht_detection;//This should never happen here in real life because such a port wouldn't even get in the ISR_WITH_DHT_port_pinmask_stack_array array
    
    *portreg_ |= eligible_devices_this_port; //Make safe and eligible pins into outputs
    *port_ &= ~( eligible_devices_this_port ); // set output pins LOW that are still eligible
        delay( 19 );

    // Here begins timing-critical code in this routine: From here to the end of timing-critical code, any changes you make to this established code result in untested timing

    *port_ = eligible_devices_this_port; // set eligible pins HIGH, still outputs
    eligible_devices_this_port &= *pinreg_;  //disqualify eligible lines that aren't high. 
    if ( !( bool ) eligible_devices_this_port ) goto this_port_done_for_dht_detection;
    *portreg_ &= ~( eligible_devices_this_port ); // change only the eligible pins to 0 ( inputs ), keep ineligible pins as they are//This line begins the problems
    *port_ |= eligible_devices_this_port; //making them have pullups so no resistors are needed to the DHT i/o pin
    turnover_reference_time = micros();
    eligible_devices_this_port &= *pinreg_;  //disqualify eligible lines that aren't high. All pins that stayed high with pullups are eligible candidates
    if ( !( bool ) eligible_devices_this_port ) goto this_port_done_for_dht_detection;
    while ( micros() - turnover_reference_time < 34 ); // device responds in reality by 8 u-sec going low ( data sheets say 20-40 uSec ).  This adds plenty of margin.
    eligible_devices_this_port &= ~( *pinreg_ );  //disqualify eligible lines that aren't low
    if ( !( bool ) eligible_devices_this_port ) goto this_port_done_for_dht_detection;
    while ( micros() - turnover_reference_time < 118 ); // device responds by 128 u-sec with good margin in time
    eligible_devices_this_port &= *pinreg_; //HIGHs on eligible pins maintain eligibility


// Endpoint of timing-critical code
this_port_done_for_dht_detection:
    *portreg_ = startstate_portreg | eligible_devices_this_port;  //Restore ports to inputs or outputs except that where devices were found become outputs regardless
    *port_ = startstate_port & ~( eligible_devices_this_port ); // set output pins LOW that are still eligible CAUSE EXTRANEOUS SYMBOLS TO APPEAR ON SERIAL LINE
    if ( ( bool ) eligible_devices_this_port ) numOfPortsWithAnyDHTDevice++;
    return ( eligible_devices_this_port );
}

u8 Portspec_ready_port_index_adjust ( u8 portindex )
{
    return( strchr( ports_string_in_heap_array, ( char )( portindex + 64 ) ) - ports_string_in_heap_array );
}

void delay_if_device_triggered( u8 pin )
{
    byte* port_ = ( byte* )portOutputRegister( digitalPinToPort( pin ) );
    byte* pinreg_ = ( byte* )portInputRegister( digitalPinToPort( pin ) );
    byte* portreg_ = ( byte* )portModeRegister( digitalPinToPort( pin ) );
    // timing-critical code: any changes you make to this established code result in untested timing
    *port_ |= digitalPinToBitMask( pin ); //making pin have pullup so no resistors are needed to the DHT i/o pin
    long turnover_reference_time = micros();
    if( !( bool )( *pinreg_ & digitalPinToBitMask( pin ) ) ) return;  //disqualify eligible lines that aren't high. All pins that stayed high with pullups are eligible candidates
    while ( micros() - turnover_reference_time < 190 )
    { 
        if( !( bool )( *pinreg_ & digitalPinToBitMask( pin ) ) ) 
        { 
            delay( 2000 );//Allow time for any DHT device to settle down
        }
    }
}

unsigned short resistor_between_LED_BUILTIN_and_PIN_A0() //default purpose for this is to signify type of line-ends to send with serial communications: MS Windows-style ( resistor connects the pins ) will include a CR with the LF, while non-windows ( no resistor ) only has the LF
{ //Need to detect any device connected to this pin and add delay for it/them
#ifndef PIN_A0
        return ( ( unsigned short ) -1 );
#else
    pinMode( LED_BUILTIN, OUTPUT );
    digitalWrite( LED_BUILTIN, HIGH );
    delay( 100 );
    pinMode( PIN_A0, OUTPUT );
//                   BEWARE:
//The following line or lines cause[ s ] any DHT device on this pin to go out of detect-ability for a while 
    digitalWrite( PIN_A0, LOW );                                                //A test here for 1 MOhm resistor between pins A0 and LED_BUILTIN while led is high:  make A0 an output, take it low, make an analog input and read it for a high
    pinMode( PIN_A0, INPUT );
    delay( 1 );                                                               //allow settling time, but not long enough to trigger a DHT device, thankfully
    if ( analogRead( PIN_A0 ) > 100 )                                           // in breadboard testing, this level needed to be at least 879 for reliable detection of a high if no delay allowance for settling time
    { 
        digitalWrite( LED_BUILTIN, LOW );                                        //A high was used to disable relays and whatnots so they don't get driven during the dht device detection process.  The circuitry for that is the end-user's responsibility
        pinMode( PIN_A0, OUTPUT );                                            
        digitalWrite( PIN_A0, HIGH );                                             //A test here for 1 MOhm resistor between pins A0 and LED_BUILTIN while led is high:  make A0 an output, take it low, make an analog input and read it for a high
        pinMode( PIN_A0, INPUT );
        unsigned short returnvalue = analogRead( PIN_A0 ) + 1;
        delay_if_device_triggered( PIN_A0 );

        pinMode( PIN_A0, OUTPUT );                                           
        digitalWrite( PIN_A0, HIGH );                                             //A test here for 1 MOhm resistor between pins A0 and LED_BUILTIN while led is high:  make A0 an output, take it low, make an analog input and read it for a high
//
        return ( returnvalue );      //Will be 1 to 101 if connected to LED_BUILTIN through resistor
    }
    else
    { 
        digitalWrite( LED_BUILTIN, LOW );                                        //A high was used to disable relays and whatnots so they don't get driven during the dht device detection process.  The circuitry for that is the end-user's responsibility
        pinMode( PIN_A0, OUTPUT );                                             
        digitalWrite( PIN_A0, HIGH );                                             //A test here for 1 MOhm resistor between pins A0 and LED_BUILTIN while led is high:  make A0 an output, take it low, make an analog input and read it for a high
        //delay( 2000 );//Allow time for any DHT device to settle down
        return ( 0 );
    }
#endif
 }

unsigned short resistor_between_LED_BUILTIN_and_PIN_A1()//default purpose for this is to signify the verbosity level end-user wants, no resistor = max verbosity, user can utilize various resistor configurations to adjust verbosity
{ 
//  The default ( no resistor ) is for maximum capability of this software product ( no memory fragmentation when devices are changed without rebooting ) which leaves a little less memory for end-user-added functionality.
//Need to detect any device connected to this pin and add delay for it/them
#ifndef PIN_A1
        return ( ( unsigned short ) -1 );
#else
    pinMode( LED_BUILTIN, OUTPUT );
    digitalWrite( LED_BUILTIN, HIGH );
    delay( 100 );
    pinMode( PIN_A1, OUTPUT );                                              
    digitalWrite( PIN_A1, LOW );                                                //A test here for 1 MOhm resistor between pins A1 and LED_BUILTIN while led is high:  make A1 an output, take it low, make an analog input and read it for a high
    pinMode( PIN_A1, INPUT );
    delay( 1 );                                                               //allow settling time
    if ( analogRead( PIN_A1 ) > 100 )                                           // in breadboard testing, this level needed to be at least 879 for reliable detection of a high if no delay allowance for settling time
    { 
        digitalWrite( LED_BUILTIN, LOW );                                        //A high was used to disable relays and whatnots so they don't get driven during the dht device detection process.  The circuitry for that is the end-user's responsibility
        pinMode( PIN_A1, OUTPUT );                                         
        digitalWrite( PIN_A1, HIGH );                                             //A test here for 1 MOhm resistor between pins A1 and LED_BUILTIN while led is high:  make A1 an output, take it low, make an analog input and read it for a high
        pinMode( PIN_A1, INPUT );
        unsigned short returnvalue = analogRead( PIN_A1 ) + 1;
        delay_if_device_triggered( PIN_A1 );
        pinMode( PIN_A1, OUTPUT );                                          
        digitalWrite( PIN_A1, HIGH );                                             //A test here for 1 MOhm resistor between pins A1 and LED_BUILTIN while led is high:  make A1 an output, take it low, make an analog input and read it for a high
        return ( returnvalue );      //Will be 1 to 101 if connected to LED_BUILTIN through resistor
    }
    else
    { 
        pinMode( PIN_A1, OUTPUT );                                             
        digitalWrite( PIN_A1, HIGH );                                             //A test here for 1 MOhm resistor between pins A1 and LED_BUILTIN while led is high:  make A1 an output, take it low, make an analog input and read it for a high
        //delay( 2000 );//Allow time for any DHT device to settle down
        digitalWrite( LED_BUILTIN, LOW );                                        //A high was used to disable relays and whatnots so they don't get driven during the dht device detection process.  The circuitry for that is the end-user's responsibility
        return ( 0 );
    }
#endif
 }

unsigned short resistor_between_LED_BUILTIN_and_PIN_A2()//default purpose for this is for end-user to indicate the host system might provide bootup configuration information.  True means to ask host and wait a short while for response
{ //Need to detect any device connected to this pin and add delay for it/them
#ifndef PIN_A2
        return ( ( unsigned short ) -1 );
#else
    pinMode( LED_BUILTIN, OUTPUT );
    digitalWrite( LED_BUILTIN, HIGH );
    delay( 100 );
    pinMode( PIN_A2, OUTPUT );                                               
    digitalWrite( PIN_A2, LOW );                                                //A test here for 1 MOhm resistor between pins A2 and LED_BUILTIN while led is high:  make A2 an output, take it low, make an analog input and read it for a high
    pinMode( PIN_A2, INPUT );
    delay( 1 );                                                               //allow settling time
    if ( analogRead( PIN_A2 ) > 100 )                                           // in breadboard testing, this level needed to be at least 879 for reliable detection of a high if no delay allowance for settling time
    { 
        digitalWrite( LED_BUILTIN, LOW );                                        //A high was used to disable relays and whatnots so they don't get driven during the dht device detection process.  The circuitry for that is the end-user's responsibility
        pinMode( PIN_A2, OUTPUT );                                             
        digitalWrite( PIN_A2, HIGH );                                             //A test here for 1 MOhm resistor between pins A2 and LED_BUILTIN while led is high:  make A2 an output, take it low, make an analog input and read it for a high
        pinMode( PIN_A2, INPUT );
        unsigned short returnvalue = analogRead( PIN_A2 ) + 1;
        delay_if_device_triggered( PIN_A2 );
        pinMode( PIN_A2, OUTPUT );                                             
        digitalWrite( PIN_A2, HIGH );                                             //A test here for 1 MOhm resistor between pins A2 and LED_BUILTIN while led is high:  make A2 an output, take it low, make an analog input and read it for a high
        return ( returnvalue );      //Will be 1 to 101 if connected to LED_BUILTIN through resistor
    }
    else
    { 
        pinMode( PIN_A2, OUTPUT );                                             
        digitalWrite( PIN_A2, HIGH );                                             //A test here for 1 MOhm resistor between pins A2 and LED_BUILTIN while led is high:  make A2 an output, take it low, make an analog input and read it for a high
        //delay( 2000 );//Allow time for any DHT device to settle down
        digitalWrite( LED_BUILTIN, LOW );                                        //A high was used to disable relays and whatnots so they don't get driven during the dht device detection process.  The circuitry for that is the end-user's responsibility
        return ( 0 );
    }
#endif
 }

bool reset_ISR_findings_and_reprobe ( bool protect_protected_pins )
{ 
  volatile u8 PCINT_pins_by_PCMSK_and_ISR[ 2 ][ 8 ][ number_of_ISRs ];
    digitalWrite( LED_BUILTIN,1 ); //This is to be used by the end user to disable relays and whatnots so they don't get driven during the device detection process.  The circuitry for that is the end-user's responsibility/discretion
    u8 number_of_ports_that_responded_to_ISR_probing = 0;  //Needs to be calculated every time the ISR probing function is executed because pins forced by momentary low Z circuit faults could be overlooked as being serviced by ISRs
    for ( u8 i = 0; i < 2; i++ )
    { 
        for ( u8 j = 0; j < 8; j++ )
        { 
            for ( u8 k = 0; k < number_of_ISRs; k++ )
            { 
                PCINT_pins_by_PCMSK_and_ISR[ i ][ j ][ k ] = ( u8 ) 0;
            }
        }
    }
    Serial.begin( _baud_rate_ ); //This speed is very dependent on the host's ability //This speed is very dependent on the host's ability
    Serial.setTimeout( 10 ); //
    while ( !Serial ) ; // wait for serial port to connect. Needed for Leonardo's native USB
    Serial.print( F( "(" ) );//This encloses characters sent to tty during test
    Serial.flush();
    Serial.end();
    byte pmask;
    u8 port_indexes_ddrmasks_and_pinlevels[ number_of_ports_found ][ 1 ][ 1 ][ 1 ]; //Not yet ready, go through all pins first and get number of ports that way
//Get ready to trip interrupts and determine which ports ( and how many for array creation ) have any PCINT lines whatsoever
    byte* portaddr;                              //byte This just gets us an index value
    byte* ddraddr;            //u8 *
    byte* pinaddr;
    number_of_ports_that_responded_to_ISR_probing = 0;
    u8 i;
    u8 ports_string_size = number_of_ports_found + 1; //ARD_TOTAL_PORTS + 1;
    char ports_with_ISRs_string[ ports_string_size ] { 0 }; //ARD_TOTAL_PORTS=11 make it 25, if=3 make it 6 ( ( ARD_TOTAL_PORTS * 2 )+( ARD_TOTAL_PORTS-8 if it is greater than 8 ) )
    char ISR_ports_with_DHTs_string[ ports_string_size ];
    char tmp_ports_under_test_string[ ports_string_size ] { 0 }; //ARD_TOTAL_PORTS=11 make it 25, if=3 make it 6 ( ( ARD_TOTAL_PORTS * 2 )+( ARD_TOTAL_PORTS-8 if it is greater than 8 ) )
    char portchar = { ' ' };
    char ISRindexchar = { ' ' };
    char ISRs_used_[ 4 ] = { "   " };
    PCICR = 0;

#ifdef PCMSK
    PCMSK = 0;
    isrspec_addr0 = &srspec[ 0 ];
    pin_change_reported_by_ISR = 0;
    Isrspec[ 0 ].mask_by_PCMSK_of_real_pins = 0;
    Isrspec[ 0 ].mask_by_PCMSK_of_valid_devices = 0;
    Isrspec[ 0 ].pcmsk = &PCMSK0;
    Isrspec[ 0 ].mask_by_port_of_current_device_being_actively_communicated_with_thisISR = 0;
    Isrspec[ 0 ].active_pin_ddr_port_reg_addr = 0;
    Isrspec[ 0 ].active_pin_output_port_reg_addr = 0;
    Isrspec[ 0 ].active_pin_pin_reg_addr = 0;
    Isrspec[ 0 ].mask_by_PCMSK_of_current_device_within_ISR = 0;
    Isrspec[ 0 ].index_in_PCMSK_of_current_device_within_ISR = 0;
    Isrspec[ 0 ].start_time_plus_max_acq_time_in_uSecs = 0;
    Isrspec[ 0 ].next_bit_coming_from_dht = 255;
    Isrspec[ 0 ].timestamps[ 0 ] = 0;
    Isrspec[ 0 ].interval = 255;
    Isrspec[ 0 ].offset = 0;
    Isrspec[ 0 ].val_tmp1 = ( unsigned short* )&Isrspec[ 0 ].sandbox_bytes[ 1 ];
    Isrspec[ 0 ].val_tmp2 = ( unsigned short* )&Isrspec[ 0 ].sandbox_bytes[ 3 ];
    Isrspec[ 0 ].millis_rest_length = Devprot[ 0 ].millis_rest_length;//make obsolete?
    for( u8 m = 0;m < sizeof( Isrspec[ 0 ].array_of_all_devspec_index_plus_1_this_ISR ) ;m++ )
        Isrspec[ 0 ].array_of_all_devspec_index_plus_1_this_ISR[ m ] = 0;
    for( u8 m = 0;m < sizeof( Isrspec[ 0 ].array_of_all_devprot_index_this_ISR ) ;m++ )
        Isrspec[ 0 ].array_of_all_devprot_index_this_ISR[ m ] = 0;
    #ifdef PCMSK0 //The purpose of this entry is for rationale only, never expected to materialize
        PCMSK0 = 0;
        isrspec_addr1 = &Isrspec[ 1 ];
        pin_change_reported_by_ISR0 = 0;
        Isrspec[ 1 ].mask_by_PCMSK_of_real_pins = 0;
        Isrspec[ 1 ].mask_by_PCMSK_of_valid_devices = 0;
        Isrspec[ 1 ].pcmsk = &PCMSK1;
        Isrspec[ 1 ].mask_by_port_of_current_device_being_actively_communicated_with_thisISR = 0;
        Isrspec[ 1 ].active_pin_ddr_port_reg_addr = 0;
        Isrspec[ 1 ].active_pin_output_port_reg_addr = 0;
        Isrspec[ 1 ].active_pin_pin_reg_addr = 0;
        Isrspec[ 1 ].mask_by_PCMSK_of_current_device_within_ISR = 0;
        Isrspec[ 1 ].index_in_PCMSK_of_current_device_within_ISR = 0;
        Isrspec[ 1 ].start_time_plus_max_acq_time_in_uSecs = 0;
        Isrspec[ 1 ].next_bit_coming_from_dht = 255;
        Isrspec[ 1 ].timestamps[ 0 ] = 0;
        Isrspec[ 1 ].interval = 255;
        Isrspec[ 1 ].offset = 0;
        Isrspec[ 1 ].val_tmp1 = ( unsigned short* )&Isrspec[ 1 ].sandbox_bytes[ 1 ];
        Isrspec[ 1 ].val_tmp2 = ( unsigned short* )&Isrspec[ 1 ].sandbox_bytes[ 3 ];
        Isrspec[ 1 ].millis_rest_length = Devprot[ 0 ].millis_rest_length;
        for( u8 m = 0;m < sizeof( Isrspec[ 1 ].array_of_all_devspec_index_plus_1_this_ISR ) ;m++ )
            Isrspec[ 1 ].array_of_all_devspec_index_plus_1_this_ISR[ m ] = 0;
        for( u8 m = 0;m < sizeof( Isrspec[ 1 ].array_of_all_devprot_index_this_ISR ) ;m++ )
            Isrspec[ 1 ].array_of_all_devprot_index_this_ISR[ m ] = 0;
    #endif
#else
    #ifdef PCMSK0
        PCMSK0 = 0;
        isrspec_addr0 = &Isrspec[ 0 ];
        Isrspec[ 0 ].mask_by_PCMSK_of_real_pins = 0;
        Isrspec[ 0 ].mask_by_PCMSK_of_valid_devices = 0;
        Isrspec[ 0 ].pcmsk = &PCMSK0;
        Isrspec[ 0 ].mask_by_port_of_current_device_being_actively_communicated_with_thisISR = 0;
        Isrspec[ 0 ].active_pin_ddr_port_reg_addr = 0;
        Isrspec[ 0 ].active_pin_output_port_reg_addr = 0;
        Isrspec[ 0 ].active_pin_pin_reg_addr = 0;
        Isrspec[ 0 ].mask_by_PCMSK_of_current_device_within_ISR = 0;
        Isrspec[ 0 ].index_in_PCMSK_of_current_device_within_ISR = 0;
        Isrspec[ 0 ].start_time_plus_max_acq_time_in_uSecs = 0;
        Isrspec[ 0 ].next_bit_coming_from_dht = 255;
        Isrspec[ 0 ].timestamps[ 0 ] = 0;
        Isrspec[ 0 ].interval = 255;
        Isrspec[ 0 ].offset = 0;
        Isrspec[ 0 ].val_tmp1 = ( unsigned short* )&Isrspec[ 0 ].sandbox_bytes[ 1 ];
        Isrspec[ 0 ].val_tmp2 = ( unsigned short* )&Isrspec[ 0 ].sandbox_bytes[ 3 ];
        Isrspec[ 0 ].millis_rest_length = Devprot[ 0 ].millis_rest_length;
        for( u8 m = 0;m < sizeof( Isrspec[ 0 ].array_of_all_devspec_index_plus_1_this_ISR ) ;m++ )
            Isrspec[ 0 ].array_of_all_devspec_index_plus_1_this_ISR[ m ] = 0;
        for( u8 m = 0;m < sizeof( Isrspec[ 0 ].array_of_all_devprot_index_this_ISR ) ;m++ )
            Isrspec[ 0 ].array_of_all_devprot_index_this_ISR[ m ] = 0;
    #endif
    #ifdef PCMSK1
        PCMSK1 = 0;
        isrspec_addr1 = &Isrspec[ 1 ];
        Isrspec[ 1 ].mask_by_PCMSK_of_real_pins = 0;
        Isrspec[ 1 ].mask_by_PCMSK_of_valid_devices = 0;
        Isrspec[ 1 ].pcmsk = &PCMSK1;
        Isrspec[ 1 ].mask_by_port_of_current_device_being_actively_communicated_with_thisISR = 0;
        Isrspec[ 1 ].active_pin_ddr_port_reg_addr = 0;
        Isrspec[ 1 ].active_pin_output_port_reg_addr = 0;
        Isrspec[ 1 ].active_pin_pin_reg_addr = 0;
        Isrspec[ 1 ].mask_by_PCMSK_of_current_device_within_ISR = 0;
        Isrspec[ 1 ].index_in_PCMSK_of_current_device_within_ISR = 0;
        Isrspec[ 1 ].start_time_plus_max_acq_time_in_uSecs = 0;
        Isrspec[ 1 ].next_bit_coming_from_dht = 255;
        Isrspec[ 1 ].timestamps[ 0 ] = 0;
        Isrspec[ 1 ].interval = 255;
        Isrspec[ 1 ].offset = 0;
        Isrspec[ 1 ].val_tmp1 = ( unsigned short* )&Isrspec[ 1 ].sandbox_bytes[ 1 ];
        Isrspec[ 1 ].val_tmp2 = ( unsigned short* )&Isrspec[ 1 ].sandbox_bytes[ 3 ];
        Isrspec[ 1 ].millis_rest_length = Devprot[ 0 ].millis_rest_length;
        for( u8 m = 0;m < sizeof( Isrspec[ 1 ].array_of_all_devspec_index_plus_1_this_ISR ) ;m++ )
            Isrspec[ 1 ].array_of_all_devspec_index_plus_1_this_ISR[ m ] = 0;
        for( u8 m = 0;m < sizeof( Isrspec[ 1 ].array_of_all_devprot_index_this_ISR ) ;m++ )
            Isrspec[ 1 ].array_of_all_devprot_index_this_ISR[ m ] = 0;
    #endif
    #ifdef PCMSK2
        PCMSK2 = 0;
        isrspec_addr2 = &Isrspec[ 2 ];
        Isrspec[ 2 ].mask_by_PCMSK_of_real_pins = 0;
        Isrspec[ 2 ].mask_by_PCMSK_of_valid_devices = 0;
        Isrspec[ 2 ].pcmsk = &PCMSK2;
        Isrspec[ 2 ].mask_by_port_of_current_device_being_actively_communicated_with_thisISR = 0;
        Isrspec[ 2 ].active_pin_ddr_port_reg_addr = 0;
        Isrspec[ 2 ].active_pin_output_port_reg_addr = 0;
        Isrspec[ 2 ].active_pin_pin_reg_addr = 0;
        Isrspec[ 2 ].mask_by_PCMSK_of_current_device_within_ISR = 0;
        Isrspec[ 2 ].index_in_PCMSK_of_current_device_within_ISR = 0;
        Isrspec[ 2 ].start_time_plus_max_acq_time_in_uSecs = 0;
        Isrspec[ 2 ].next_bit_coming_from_dht = 255;
        Isrspec[ 2 ].timestamps[ 0 ] = 0;
        Isrspec[ 2 ].interval = 255;
        Isrspec[ 2 ].offset = 0;
        Isrspec[ 2 ].val_tmp1 = ( unsigned short* )&Isrspec[ 2 ].sandbox_bytes[ 1 ];
        Isrspec[ 2 ].val_tmp2 = ( unsigned short* )&Isrspec[ 2 ].sandbox_bytes[ 3 ];
        Isrspec[ 2 ].millis_rest_length = Devprot[ 0 ].millis_rest_length;
        for( u8 m = 0;m < sizeof( Isrspec[ 2 ].array_of_all_devspec_index_plus_1_this_ISR ) ;m++ )
            Isrspec[ 2 ].array_of_all_devspec_index_plus_1_this_ISR[ m ] = 0;
        for( u8 m = 0;m < sizeof( Isrspec[ 2 ].array_of_all_devprot_index_this_ISR ) ;m++ )
            Isrspec[ 2 ].array_of_all_devprot_index_this_ISR[ m ] = 0;
    #endif
    #ifdef PCMSK3
        PCMSK3 = 0;
        isrspec_addr3 = &Isrspec[ 3 ];
        Isrspec[ 3 ].mask_by_PCMSK_of_real_pins = 0;
        Isrspec[ 3 ].mask_by_PCMSK_of_valid_devices = 0;
        Isrspec[ 3 ].pcmsk = &PCMSK3;
        Isrspec[ 3 ].mask_by_port_of_current_device_being_actively_communicated_with_thisISR = 0;
        Isrspec[ 3 ].active_pin_ddr_port_reg_addr = 0;
        Isrspec[ 3 ].active_pin_output_port_reg_addr = 0;
        Isrspec[ 3 ].active_pin_pin_reg_addr = 0;
        Isrspec[ 3 ].mask_by_PCMSK_of_current_device_within_ISR = 0;
        Isrspec[ 3 ].index_in_PCMSK_of_current_device_within_ISR = 0;
        Isrspec[ 3 ].start_time_plus_max_acq_time_in_uSecs = 0;
        Isrspec[ 3 ].next_bit_coming_from_dht = 255;
        Isrspec[ 3 ].timestamps[ 0 ] = 0;
        Isrspec[ 3 ].interval = 255;
        Isrspec[ 3 ].offset = 0;
        Isrspec[ 3 ].val_tmp1 = ( unsigned short* )&Isrspec[ 3 ].sandbox_bytes[ 1 ];
        Isrspec[ 3 ].val_tmp2 = ( unsigned short* )&Isrspec[ 3 ].sandbox_bytes[ 3 ];
        Isrspec[ 3 ].millis_rest_length = Devprot[ 0 ].millis_rest_length;
        for( u8 m = 0;m < sizeof( Isrspec[ 3 ].array_of_all_devspec_index_plus_1_this_ISR ) ;m++ )
            Isrspec[ 3 ].array_of_all_devspec_index_plus_1_this_ISR[ m ] = 0;
        for( u8 m = 0;m < sizeof( Isrspec[ 3 ].array_of_all_devprot_index_this_ISR ) ;m++ )
            Isrspec[ 3 ].array_of_all_devprot_index_this_ISR[ m ] = 0;
    #endif
#endif
    digitalWrite( LED_BUILTIN, HIGH );                                           //A high is used to disable relays and whatnots so they don't get driven during the dht device detection process.  The circuitry for that is the end-user's responsibility
    delay( 100 ); //Allow time for even a mechanical relay to operate

    bool pinmode[ sizeof( pins_NOT_safe_to_toggle_during_testing ) ];
    bool pinstate[ sizeof( pins_NOT_safe_to_toggle_during_testing ) ];

    for ( u8 f = 0; f < sizeof( pins_NOT_safe_to_toggle_during_testing ); f++ ) //This loop appears to set protected pins only to outputs
    { //what we need is to set all real pins, skipping not_low_Z pins, to outputs while making the protected ones keep their level, safe ones be high.
//Then end this for loop and start another to cycle through all pins, skipping the "unsafe for any reason" ones, making each safe pin low, then high and check for interrupt
//After entire interrupt pins loop completed, pins with DHT devices on them need to be set to output, low.
//Store initial mode and state of semi-protected pins only
    pinmode[ f ] = ( bool )( ( byte )*portModeRegister( digitalPinToPort( pins_NOT_safe_to_toggle_during_testing[ f ] ) ) \
    & ( byte )digitalPinToBitMask( pins_NOT_safe_to_toggle_during_testing[ f ] ) );
    pinstate[ f ] = ( bool )( digitalRead( pins_NOT_safe_to_toggle_during_testing[ f ] ) );
    }

    for ( u8 pin = 0; pin < NUM_DIGITAL_PINS; pin++ )
    {
        if ( !pin_NOT_safe_even_to_make_low_Z_during_testing( pin ) ) //should only check for membership in pins_NOT_safe_even_to_make_low_Z_during_testing
        { 
            volatile u8* _portddr = portModeRegister( digitalPinToPort( pin ) );
            volatile u8* _port = portOutputRegister( digitalPinToPort( pin ) );
            bool pinlevel = ( bool ) ( *portInputRegister( digitalPinToPort( pin ) ) & digitalPinToBitMask( pin ) ); // store the level of the pin
            if ( pinlevel )
            { 
              *_port = *portInputRegister( digitalPinToPort( pin ) ) | digitalPinToBitMask( pin ); 
            }
            else
            { 
              *_port = *portInputRegister( digitalPinToPort( pin ) ) & ~digitalPinToBitMask( pin ); //set the pin to the level read before
            }
            *_portddr |= digitalPinToBitMask( pin ); //Makes the pin an output
            if ( pinlevel )
            { 
              *_port = *portInputRegister( digitalPinToPort( pin ) ) | digitalPinToBitMask( pin ); 
            }
            else
            { 
              *_port = *portInputRegister( digitalPinToPort( pin ) ) & ~digitalPinToBitMask( pin ); //set the pin to the level read before
            }
        }
    }
    for ( u8 pin = 0; pin < NUM_DIGITAL_PINS; pin++ ) //See if pin is serviced by an ISR( PCINTn_vect ).  Build port masks and PCMSK, PCICR masks.  Skip unsafe pins
    {
        byte original_port_levels;
            for ( u8 f = 0; f < sizeof( pins_NOT_safe_even_to_make_low_Z_during_testing ); f++ )
            { 
                if ( pin == pins_NOT_safe_even_to_make_low_Z_during_testing[ f ] )
                { 
                    goto EndOfThisPin;
                }
            }
        
        
            for ( u8 f = 0; f < sizeof( pins_NOT_safe_to_toggle_during_testing ); f++ )
            { 
                if ( pin == pins_NOT_safe_to_toggle_during_testing[ f ] )
                { 
                    goto EndOfThisPin;
                }
            }
        portaddr = ( byte* )portOutputRegister( digitalPinToPort( pin ) );
        ddraddr = ( byte* )portModeRegister( digitalPinToPort( pin ) );
        pinaddr = ( byte* )portInputRegister( digitalPinToPort( pin ) );
        portchar = ( char ) ( digitalPinToPort( pin ) + 64 );                        //Compute the alpha of the port
        original_port_levels = *pinaddr;
        for ( byte mask = 1; mask != 0; mask <<= 1 ) //PCICR != 4 || mask != 0;mask <<= 1 )
        {
            pmask = digitalPinToBitMask( pin );
#ifdef PCMSK
            PCMSK = mask;
    #ifdef PCMSK0 //The purpose of this entry is for rationale only, never expected to materialize
                PCMSK0 = mask;
    #endif
#else
    #ifdef PCMSK0
                PCMSK0 = mask;
    #endif
    #ifdef PCMSK1
                PCMSK1 = mask;
    #endif
    #ifdef PCMSK2
                PCMSK2 = mask;
    #endif
    #ifdef PCMSK3
                PCMSK3 = mask;
    #endif
#endif
            *portaddr &= ~bit( digitalPinToBitMask( pin ) ); //makes pin low, but requires too much delay?  We're trying it again
            delayMicroseconds( 2 ); //Empirically determined.  Circuit capacitance could make this value insufficient.  Not needed for Leonardo but needed for Uno
#ifdef PCMSK
            PCIFR |= B1;
    #ifdef PCMSK0 //The purpose of this entry is for rationale only, never expected to materialize
            PCIFR |= B11;
    #endif
#else
    #ifdef PCMSK0
            PCIFR |= B1;
    #endif
    #ifdef PCMSK1
            PCIFR |= B11;
    #endif
    #ifdef PCMSK2
            PCIFR |= B111;
    #endif
    #ifdef PCMSK3
            PCIFR |= B1111;
    #endif
#endif
//            PCIFR |= B111; //Doesn't help anything in bench environment whenever we've needed help here and there. It clears the PC-Interrupts flags for noisy environments in the field.
            cli();
#ifdef PCMSK
            PCICR = B1;
    #ifdef PCMSK0 //The purpose of this entry is for rationale only, never expected to materialize
            PCICR = B11;
    #endif
#else
    #ifdef PCMSK0
            PCICR = B1;
    #endif
    #ifdef PCMSK1
            PCICR = B11;
    #endif
    #ifdef PCMSK2
            PCICR = B111;
    #endif
    #ifdef PCMSK3
            PCICR = B1111;
    #endif
#endif
            *pinaddr = pmask; //toggle pin state
            delayMicroseconds( 20 ); //Empirically determined, value is dependent on circuit capacitance and required signal characteristics.
            PCICR = 0;
            byte catchPCIs = PCIFR & B1111;
            PCIFR |= B1111;
            sei();
//Put the pin levels back the way they were
               if ( ( bool ) ( original_port_levels | digitalPinToBitMask( pin ) ) )
            { 
                *portaddr = *portInputRegister( digitalPinToPort( pin ) ) | digitalPinToBitMask( pin ); 
            }
            else
            { 
                *portaddr = *portInputRegister( digitalPinToPort( pin ) ) & ~digitalPinToBitMask( pin ); //set the pin to the level read before
            }
            u8 position_of_port_index = 0;
            for( u8 devspec_index = 0; devspec_index < ( ( long unsigned int )ports_string_in_heap_array - ( long unsigned int )Devspec ) / sizeof( DEVSPEC ); devspec_index++ )
            {
                if( Devspec[ devspec_index ].Dpin == pin )
                {
                    unsigned long timenow = millis();//A single point of reference to prevent changing during the following
                    Devspec[ devspec_index ].timestamp_of_pin_last_attempted_device_read_millis = 0;
                    Devspec[ devspec_index ].device_busy_resting_this_more_millis = Devprot[ Devspec[ devspec_index ].devprot_index ].millis_rest_length;
                    break;
                }
            }
            if ( catchPCIs == 0 ) goto intcatchdone; //Detour if no INTs occurred
            mask = 0;
/*
//Now could make our own defines so they are correct: not tested

#define digitalPinToPCICR( p )    ( ( ( p ) >= 0 && ( p ) <= 21 ) ? ( &PCICR ) : ( ( u8 * )0 ) )
#define digitalPinToPCICRbit( p ) ( ( ( p ) <= 7 ) ? 2 : ( ( ( p ) <= 13 ) ? 0 : 1 ) )
#define digitalPinToPCMSK( p )    ( ( ( p ) <= 7 ) ? ( &PCMSK2 ) : ( ( ( p ) <= 13 ) ? ( &PCMSK0 ) : ( ( ( p ) <= 21 ) ? ( &PCMSK1 ) : ( ( u8 * )0 ) ) ) )
#define digitalPinToPCMSKbit( p ) ( ( ( p ) <= 7 ) ? ( p ) : ( ( ( p ) <= 13 ) ? ( ( p ) - 8 ) : ( ( p ) - 14 ) ) )

#define digitalPinToInterrupt( p )  ( ( p ) == 2 ? 0 : ( ( p ) == 3 ? 1 : NOT_AN_INTERRUPT ) )
*/
            //These lines now are executed if any interrupt occurred on this pin
// **********************************An interrupt occurred on this pin**********************************************
//  find the matching port looking up the index by search for it through
/*
 * Goal is to enable later minimizing memory useage by only storing ports, pins, and isrs that have DHT devices on them, let's call it sparsing the array
 * That means the indexes will no longer conform to the original board-defined indexes but rather to found order ( pick order ) by pin number traverse order
 * The ISR list starts in ISR index order for the purpose of ISR discovery, but must end up different as well.  It won't be in pick order necessarily, 
 * but holes in the index continuum for ISRs must be removed and thereafter ISR array indexes will be decreased
 * PORT INDEXES OF THE ARRAY ARE ARRANGED IN PICK-ORDER, NOT BOARD PORT INDEX ORDER
 * 
 * to get to proper port entry in array only having the original board-defined index, we get that index by
 */
//JUST DON'T FORGET THAT THE PORT MAY NOT HAVE AN ARRAY ENTRY IF NO DHT DEVICES WERE DISCOVERED ON IT?
//FIGURE OUT IF WE NEED TO KEEP THE ISR_WITH_DHT_port_pinmask_stack_array ARRAY NEXT... NOT NEEDED HERE!
//following lines: Save the port-pin mask of the pin in its Portspec/Isrspec/Pinspec to indicate this bitmask of this pin's port is served by an ISR
// position_of_port_index = index of port in Portspec  
//Save the port-pin mask of the pin to indicate this bitmask of this pin's port is served by an ISR
            cli(); //Atomic read: Because variables checked below could become multi-byte in an interruptable code environment in a later version.  Irrelevant in this specific version, but giving recognition to the fact anyway
    #ifdef PCMSK
            if ( catchPCIs & B1 )
            { 
                Isrspec[ 0 ].mask_by_PCMSK_of_real_pins |= PCMSK;
                u8 devspec_index = 0;
                for( ; devspec_index < ( ( long unsigned int )ports_string_in_heap_array - ( long unsigned int )Devspec ) / sizeof( DEVSPEC ); devspec_index++ )
                {
                    if( Devspec[ devspec_index ].Dpin == pin )
                    { 
                        Isrspec[ 0 ].mask_by_PCMSK_of_valid_devices |= PCMSK;    //record a device found at this PCMSK bit
                        Devspec[ devspec_index ].my_isrspec_addr = &Isrspec[ 0 ];
                        break;
                    }
                }
                u8 indexwisePCMSK = 0;//make sure the scope extends outside the next for loop
                for ( ;PCMSK >>= 1; indexwisePCMSK++ );// converted to an index 0-7;
                if( Devspec[ devspec_index ].Dpin == pin ) 
                {
                    Isrspec[ 0 ].array_of_all_devspec_index_plus_1_this_ISR[ indexwisePCMSK ] = devspec_index + 1;//index_in_PCMSK_of_current_device_within_ISR;
                    if( !Isrspec[ 0 ].index_in_PCMSK_of_current_device_within_ISR )
                    {
                        Isrspec[ 0 ].index_in_PCMSK_of_current_device_within_ISR = indexwisePCMSK;
                        Isrspec[ 0 ].mask_by_PCMSK_of_current_device_within_ISR = PCMSK;
                    }
                }
                // Need to store this in an array for this ISR, ordered by PCMSK bit position, will store pin number from which port and bitmask will be obtained as needed
                if ( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ indexwisePCMSK ][ 0 ] == NULL ) PCINT_pins_by_PCMSK_and_ISR[ 0 ][ indexwisePCMSK ][ 0 ] = pin + 1;
                else PCINT_pins_by_PCMSK_and_ISR[ 1 ][ indexwisePCMSK ][ 0 ] = pin + 1;
            }
    #endif
    #ifdef PCMSK0
        #ifdef PCMSK
            if ( catchPCIs & B10 )
            { 
                Isrspec[ 1 ].mask_by_PCMSK_of_real_pins |= PCMSK0;
                u8 devspec_index = 0;
                for( ; devspec_index < ( ( long unsigned int )ports_string_in_heap_array - ( long unsigned int )Devspec ) / sizeof( DEVSPEC ); devspec_index++ )
                {
                    if( Devspec[ devspec_index ].Dpin == pin )
                    {
                        Isrspec[ 1 ].mask_by_PCMSK_of_valid_devices |= PCMSK0;    //record a device found at this PCMSK bit
                        Devspec[ devspec_index ].my_isrspec_addr = &Isrspec[ 1 ];
                        break;
                    }
                }
        #else
            if ( catchPCIs & B1 ) //This B1 limits the use of PCMSK0 to systems where PCMSK does not exist
            { //Isrspec[ ].mask_by_PCMSK_of_valid_devices needs to get set here as well
                Isrspec[ 0 ].mask_by_PCMSK_of_real_pins |= PCMSK0;
                u8 devspec_index = 0;
                for( ; devspec_index < ( ( long unsigned int )ports_string_in_heap_array - ( long unsigned int )Devspec ) / sizeof( DEVSPEC ); devspec_index++ )
                {
                    if( Devspec[ devspec_index ].Dpin == pin )
                    {
                        Isrspec[ 0 ].mask_by_PCMSK_of_valid_devices |= PCMSK0;    //record a device found at this PCMSK bit
                        Devspec[ devspec_index ].my_isrspec_addr = &Isrspec[ 0 ];
                        break;
                    }
                }
        #endif
                u8 indexwisePCMSK0 = 0;//make sure the scope extends outside the next for loop
                for ( ;PCMSK0 >>= 1; indexwisePCMSK0++ );// converted to an index 0-7;
        #ifdef PCMSK
                if( Devspec[ devspec_index ].Dpin == pin )
                {
                    Isrspec[ 1 ].array_of_all_devspec_index_plus_1_this_ISR[ indexwisePCMSK0 ] = devspec_index + 1;
                    if( !Isrspec[ 1 ].index_in_PCMSK_of_current_device_within_ISR )
                    {
                        Isrspec[ 1 ].index_in_PCMSK_of_current_device_within_ISR = indexwisePCMSK0;
                        Isrspec[ 1 ].mask_by_PCMSK_of_current_device_within_ISR = PCMSK0;
                    }
                }
                if ( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ indexwisePCMSK0 ][ 1 ] == NULL ) PCINT_pins_by_PCMSK_and_ISR[ 0 ][ indexwisePCMSK0 ][ 1 ] = pin + 1;
                else PCINT_pins_by_PCMSK_and_ISR[ 1 ][ indexwisePCMSK0 ][ 1 ] = pin + 1;
        #else
                if( Devspec[ devspec_index ].Dpin == pin )
                {
                    Isrspec[ 0 ].array_of_all_devspec_index_plus_1_this_ISR[ indexwisePCMSK0 ] = devspec_index + 1;
                    if( !Isrspec[ 0 ].index_in_PCMSK_of_current_device_within_ISR )
                    {
                        Isrspec[ 0 ].index_in_PCMSK_of_current_device_within_ISR = indexwisePCMSK0;
                        Isrspec[ 0 ].mask_by_PCMSK_of_current_device_within_ISR = PCMSK0;
                    }
                }
                if ( !PCINT_pins_by_PCMSK_and_ISR[ 0 ][ indexwisePCMSK0 ][ 0 ] ) PCINT_pins_by_PCMSK_and_ISR[ 0 ][ indexwisePCMSK0 ][ 0 ] = pin + 1;
                else PCINT_pins_by_PCMSK_and_ISR[ 1 ][ indexwisePCMSK0 ][ 0 ] = pin + 1;
        #endif
            }
    #endif
    #ifdef PCMSK1
            if ( catchPCIs & B10 )
            { 
                Isrspec[ 1 ].mask_by_PCMSK_of_real_pins |= PCMSK1;
                u8 devspec_index = 0;
                for( ; devspec_index < ( ( long unsigned int )ports_string_in_heap_array - ( long unsigned int )Devspec ) / sizeof( DEVSPEC ); devspec_index++ )
                {
                    if( Devspec[ devspec_index ].Dpin == pin )
                    {
                        Isrspec[ 1 ].mask_by_PCMSK_of_valid_devices |= PCMSK1;    //record a device found at this PCMSK bit
                        Devspec[ devspec_index ].my_isrspec_addr = &Isrspec[ 1 ];
                        break;
                    }
                }
                u8 indexwisePCMSK1 = 0;//make sure the scope extends outside the next for loop
                for ( ;PCMSK1 >>= 1; indexwisePCMSK1++ );// converted to an index 0-7;
                if( Devspec[ devspec_index ].Dpin == pin )
                {
                    Isrspec[ 1 ].array_of_all_devspec_index_plus_1_this_ISR[ indexwisePCMSK1 ] = devspec_index + 1;
                    if( !Isrspec[ 1 ].index_in_PCMSK_of_current_device_within_ISR )
                    {
                        Isrspec[ 1 ].index_in_PCMSK_of_current_device_within_ISR = indexwisePCMSK1;
                        Isrspec[ 1 ].mask_by_PCMSK_of_current_device_within_ISR = PCMSK1;
                    }
                }
                if ( !PCINT_pins_by_PCMSK_and_ISR[ 0 ][ indexwisePCMSK1 ][ 1 ] )  PCINT_pins_by_PCMSK_and_ISR[ 0 ][ indexwisePCMSK1 ][ 1 ] = pin + 1;
                else PCINT_pins_by_PCMSK_and_ISR[ 1 ][ indexwisePCMSK1 ][ 1 ] = pin + 1;
            }
    #endif
    #ifdef PCMSK2
            if ( catchPCIs & B100 )
            {
                Isrspec[ 2 ].mask_by_PCMSK_of_real_pins |= PCMSK2;
                u8 devspec_index = 0;
                for( ; devspec_index < ( ( long unsigned int )ports_string_in_heap_array - ( long unsigned int )Devspec ) / sizeof( DEVSPEC ); devspec_index++ )
                {
                    if( Devspec[ devspec_index ].Dpin == pin )
                    {
                        Isrspec[ 2 ].mask_by_PCMSK_of_valid_devices |= PCMSK2;    //record a device found at this PCMSK bit
                        Devspec[ devspec_index ].my_isrspec_addr = &Isrspec[ 2 ];
                        break;
                    }
                }

                u8 indexwisePCMSK2 = 0;//make sure the scope extends outside the next for loop
                for ( ;PCMSK2 >>= 1; indexwisePCMSK2++ );// converted to an index 0-7;
                if( Devspec[ devspec_index ].Dpin == pin )
                {
                    Isrspec[ 2 ].array_of_all_devspec_index_plus_1_this_ISR[ indexwisePCMSK2 ] = devspec_index + 1;
                    if( !Isrspec[ 2 ].index_in_PCMSK_of_current_device_within_ISR )
                    {
                        Isrspec[ 2 ].index_in_PCMSK_of_current_device_within_ISR = indexwisePCMSK2;
                        Isrspec[ 2 ].mask_by_PCMSK_of_current_device_within_ISR = PCMSK2;
                    }
                }
                if ( !PCINT_pins_by_PCMSK_and_ISR[ 0 ][ indexwisePCMSK2 ][ 2 ] )  PCINT_pins_by_PCMSK_and_ISR[ 0 ][ indexwisePCMSK2 ][ 2 ] = pin + 1;
                else PCINT_pins_by_PCMSK_and_ISR[ 1 ][ indexwisePCMSK2 ][ 2 ] = pin + 1;
            }
    #endif
    #ifdef PCMSK3
            if ( catchPCIs & B1000 )
            {
                Isrspec[ 3 ].mask_by_PCMSK_of_real_pins |= PCMSK3;
                u8 devspec_index = 0;
                for( ; devspec_index < ( ( long unsigned int )ports_string_in_heap_array - ( long unsigned int )Devspec ) / sizeof( DEVSPEC ); devspec_index++ )
                {
                    if( Devspec[ devspec_index ].Dpin == pin )
                    {
                        Isrspec[ 3 ].mask_by_PCMSK_of_valid_devices |= PCMSK3;    //record a device found at this PCMSK bit
                        Devspec[ devspec_index ].my_isrspec_addr = &Isrspec[ 3 ];
                        break;
                    }
                }
                u8 indexwisePCMSK3 = 0;//make sure the scope extends outside the next for loop
                for ( ;PCMSK3 >>= 1; indexwisePCMSK3++ );// converted to an index 0-7;
                if( Devspec[ devspec_index ].Dpin == pin )
                {
                    Isrspec[ 3 ].array_of_all_devspec_index_plus_1_this_ISR[ indexwisePCMSK3 ] = devspec_index + 1;
                    if( !Isrspec[ 3 ].index_in_PCMSK_of_current_device_within_ISR )
                    {
                        Isrspec[ 3 ].index_in_PCMSK_of_current_device_within_ISR = indexwisePCMSK3;
                        Isrspec[ 3 ].mask_by_PCMSK_of_current_device_within_ISR = PCMSK3;
                    }
                }
                if ( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ indexwisePCMSK3 ][ 3 ] == NULL )  PCINT_pins_by_PCMSK_and_ISR[ 0 ][ indexwisePCMSK3 ][ 3 ] = pin + 1;
                else PCINT_pins_by_PCMSK_and_ISR[ 1 ][ indexwisePCMSK3 ][ 3 ] = pin + 1;
            }
    #endif
            sei();
intcatchdone:;
            cli();                                                  // If stopping all interrupts like this becomes problematic, we'll instead monitor what the ISRs do to the portspec array
            //All Interrupt "ears" open for any activated DHT devices to be noticed
#ifdef PCMSK
            PCICR = B1;
            PCMSK = 255;
    #ifdef PCMSK0 //The purpose of this entry is for rationale only, never expected to materialize
            PCICR = B11;
            PCMSK0 = 255;
    #endif
#else
    #ifdef PCMSK0
            PCICR = B1;
            PCMSK0 = 255;
    #endif
    #ifdef PCMSK1
            PCICR = B11;
            PCMSK1 = 255;
    #endif
    #ifdef PCMSK2
            PCICR = B111;
            PCMSK2 = 255;
    #endif
    #ifdef PCMSK3
            PCICR = B1111;
            PCMSK3 = 255;
    #endif
#endif
            delayMicroseconds( 270 ); //allow enough time for any activated DHT devices to send their next bit transition
            while( PCIFR & 15 != 0 )
            { //If any DHT devices were hit, wait here until they stop transmitting.  Each bit will be less than 190 uS
                PCIFR |= B1111;
                delayMicroseconds( 270 );
            }
            PCICR = 0;
#ifdef PCMSK
            PCMSK = 0;
#endif
#ifdef PCMSK0
            PCMSK0 = 0;
#endif
#ifdef PCMSK1
            PCMSK1 = 0;
#endif
#ifdef PCMSK2
            PCMSK2 = 0;
#endif
#ifdef PCMSK3
            PCMSK3 = 0;
#endif
            sei();//Allow interrupts again

        } //End of mask bit step for-loop  EDITOR IS WRONG ABOUT SHOWING THIS B/C IT DOESN'T KNOW OUTCOME OF IFDEFs
        

EndOfThisPin:;
    } //Here is end of do-every-pin loop.  EDITOR IS WRONG ABOUT SHOWING THIS B/C IT DOESN'T KNOW OUTCOME OF IFDEFs    ONLY HERE CAN WE KNOW HOW MANY PORTS SUPPLYING ISRs THERE ARE - number_of_ports_with_functioning_DHT_devices_and_serviced_by_ISR

//Here we must restore previous pin settings, skipping the protected pins altogether
    for ( u8 pin = 0; pin < NUM_DIGITAL_PINS; pin++ )
    { 
        bool this_pin_protected = false;
        for ( u8 f = 0; f < sizeof( pins_NOT_safe_to_toggle_during_testing ); f++ )
        { 
            if ( pin == pins_NOT_safe_to_toggle_during_testing[ f ] )
            { 
                this_pin_protected = true;
                if ( pinmode[ f ] == OUTPUT ) digitalWrite( pin, pinstate[ f ] );
                pinMode( pin, pinmode[ f ] );
                if ( pinmode[ f ] == OUTPUT ) digitalWrite( pin, pinstate[ f ] );
            }
        }
        //Now we set all PCINT pins that have devices to output, low
        if ( !this_pin_protected )
        { 
            u8 a = 0;
            u8 b = 0;
            u8 c = 0;
            while( PCINT_pins_by_PCMSK_and_ISR[ a++ ][ b ][ c ] != pin + 1 && PCINT_pins_by_PCMSK_and_ISR[ a ][ b++ ][ c ] != pin + 1 )
            { 
                a = 0;
                b = b%8;
                if ( !( bool )b ) 
                { 
                    c = ( c + 1 ) % number_of_ISRs;
                    if ( !( bool )c ) break;
                }
            }
            if( !( a == b == c == 0 ) )
            { 
                pinMode( pin, OUTPUT );
                digitalWrite( pin, LOW );
            }
        }
    }
    digitalWrite( LED_BUILTIN, LOW ); //Telling any supporting circuitry ( if end-user added any ) that ISR discovery is done, so pins can now be connected on through to the field
    Serial.begin( _baud_rate_ ); //This speed is very dependent on the host's ability //This speed is very dependent on the host's ability
    Serial.setTimeout( 10 ); //
    while ( !Serial ) ; // wait for serial port to connect. Needed for Leonardo's native USB
    Serial.print( F( "<-Ignore any extraneous characters here)" ) );
    bool any_wrong_digitalPinToPCICRbit_reports = false;
    bool any_wrong_digitalPinToPCMSKbit_reports = false;
    for ( u8 j = 0; j < number_of_ISRs; j++ )
    {
        Serial.println(); 
        Serial.println(); 
        Serial.print( F( "For this ISR ( ISR" ) );
        Serial.print( j );
        Serial.print( F( " with PCMSK" ) );
#ifndef PCMSK
        Serial.print( j );
#else
        if ( j > 0 )
            Serial.print( j-1 );
#endif
        Serial.print( F( " ), each PCMSK bit showing the pins that will trigger a pin change interrupt on it:" ) );
        Serial.println();
        for ( u8 i = 0; i < 8; i++ )
        {
            Serial.print( i );
            Serial.print( F( ": " ) );
            if ( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ j ] )//true for every pin having an ISR
            { //real pin number = PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ j ] - 1, real ISR number = j, real PCMSK bit index = i
                Serial.print( F( "Can be triggered by each voltage toggle occurring on D" ) );
                Serial.print( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ j ] - 1 );
                if ( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ j ] < 101 ) Serial.print( F( " " ) );
                if ( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ j ] < 11 ) Serial.print( F( " " ) );
                print_analog_if_exists( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ j ] - 1 );
                Serial.print( F( "( port PORT" ) );
                Serial.print( ( char ) ( digitalPinToPort( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ j ] - 1 ) + 64 ) );                   //By adding 64, this gets an alpha from the index, index 01 = A
                Serial.print( F( " bit mask " ) );
                Serial.print( digitalPinToBitMask( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ j ] - 1 ), BIN );
                Serial.print( F( " )" ) );
                if ( PCINT_pins_by_PCMSK_and_ISR[ 1 ][ i ][ j ] ) 
                { 
                    if( digitalPinToPort( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ j ] - 1 ) != digitalPinToPort( PCINT_pins_by_PCMSK_and_ISR[ 1 ][ i ][ j ] - 1 ) )
                        Serial.print( F( " conflicts with D" ) );
                    else
                        Serial.print( F( " and on D" ) );
                    Serial.print( PCINT_pins_by_PCMSK_and_ISR[ 1 ][ i ][ j ] - 1 );
                    Serial.print( F( " " ) );
//                    Serial.print( F( ". Pinxref->PIN_xref[ PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ j ] - 1 ] = " ) );
//                    Serial.print( Pinxref->PIN_xref[ PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ j ] - 1 ] );
                    print_analog_if_exists( PCINT_pins_by_PCMSK_and_ISR[ 1 ][ i ][ j ] - 1 );
                    Serial.print( F( " ( port PORT" ) );
                    Serial.print( ( char ) ( digitalPinToPort( PCINT_pins_by_PCMSK_and_ISR[ 1 ][ i ][ j ] - 1 ) + 64 ) );
                    Serial.print( F( " bit mask " ) );
                    Serial.print( digitalPinToBitMask( PCINT_pins_by_PCMSK_and_ISR[ 1 ][ i ][ j ] - 1 ), BIN );
                    Serial.print( F( " )" ) );
                    if( digitalPinToPort( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ j ] - 1 ) != digitalPinToPort( PCINT_pins_by_PCMSK_and_ISR[ 1 ][ i ][ j ] - 1 ) )
                        Serial.print( F( " only the first pin listed can be used with confidence with this software product" ) );
                }
                if( digitalPinToPCICRbit( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ j ] - 1 ) != j )
                { 
                    Serial.print( F( "!" ) );
                    any_wrong_digitalPinToPCICRbit_reports = true;
                }
                if( digitalPinToPCMSKbit( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ j ] - 1 ) != i )
                { 
                    Serial.print( F( "*" ) );
                    any_wrong_digitalPinToPCMSKbit_reports = true;
                }
            }
            else Serial.print( F( "No PCINT-to-pin connection or the supported pin is declared protected" ) );
            Serial.println(); 
        }
    }
#ifdef PCMSK4
    Serial.println( F( "At least one other ISR is available" ) );
#endif
    Serial.println(); 
    Serial.print( F( "Summary of ISR-to-pin information:" ) );
    Serial.println(); 
#ifdef PCMSK
    Serial.print( F( "ISR - D-pins by PCMSK bit" ) );         // j is ISR number
    #ifdef PCMSK0
        Serial.print( F( "    " ) );
    #endif
#endif
#ifdef PCMSK0
    Serial.print( F( "ISR0 - D-pins by PCMSK0 bit" ) );
#endif
#ifdef PCMSK1
    Serial.print( F( "       ISR1 - D-pins by PCMSK1 bit" ) );
#endif
#ifdef PCMSK2
    Serial.print( F( "       ISR2 - D-pins by PCMSK2 bit" ) );         // j is ISR number
#endif
#ifdef PCMSK3
    Serial.print( F( "       ISR3 - D-pins by PCMSK3 bit" ) );         // j is ISR number
#endif

    for ( u8 i = 0; i < 8; i++ )                                                                                         // i is PCMSK bit
    {
        Serial.println(); 
        Serial.print( i );
        Serial.print( F( ": " ) );
#if defined ( PCMSK ) || defined ( PCMSK0 )
        if ( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ 0 ] ) 
        { 
            Serial.print( F( "D" ) );
            Serial.print( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ 0 ] - 1 );
            if ( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ 0 ] < 101 ) Serial.print( F( " " ) );
            if ( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ 0 ] < 11 ) Serial.print( F( " " ) );
            print_analog_if_exists( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ 0 ] - 1 );
            if( digitalPinToPCICRbit( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ 0 ] - 1 ) != 0 )
            { 
                Serial.print( F( "!" ) );
                any_wrong_digitalPinToPCICRbit_reports = true;
            }
            else Serial.print( F( " " ) );
            if    ( digitalPinToPCMSKbit( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ 0 ] - 1 ) != i )
            { 
                Serial.print( F( "*" ) );
                any_wrong_digitalPinToPCMSKbit_reports = true;
            }
            else Serial.print( F( " " ) );
            if ( ( bool )( Isrspec[ 0 ].mask_by_PCMSK_of_valid_devices & 1<<i ) )
            {
                pinMode( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ 0 ] - 1, OUTPUT );
                digitalWrite( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ 0 ] - 1, HIGH ); //This places the level on the DHT devices that they need to be prepared to imminently and immediately send their data, assuming an adequate rest period
                Serial.print( F( "DHT" ) );
            }
            else
            {
                Serial.print( F( "   " ) );
                if( !pin_in_protected_arrays( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ 0 ] - 1 ) ) pinMode( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ 0 ] - 1, INPUT ); 
            }
            if ( PCINT_pins_by_PCMSK_and_ISR[ 1 ][ i ][ 0 ] ) 
            { 
                Serial.print( F( " and D" ) );
                Serial.print( PCINT_pins_by_PCMSK_and_ISR[ 1 ][ i ][ 0 ] - 1 ); 
                if ( PCINT_pins_by_PCMSK_and_ISR[ 1 ][ i ][ 0 ] < 101 ) Serial.print( F( " " ) );
                if ( PCINT_pins_by_PCMSK_and_ISR[ 1 ][ i ][ 0 ] < 11 ) Serial.print( F( " " ) );
                print_analog_if_exists( PCINT_pins_by_PCMSK_and_ISR[ 1 ][ i ][ 0 ] - 1 );
                if( digitalPinToPCICRbit( PCINT_pins_by_PCMSK_and_ISR[ 1 ][ i ][ 0 ] - 1 ) != 0 )
                { 
                    Serial.print( F( "!" ) );
                    any_wrong_digitalPinToPCICRbit_reports = true;
                }
                else Serial.print( F( " " ) );
                if    ( i != digitalPinToPCICRbit( PCINT_pins_by_PCMSK_and_ISR[ 1 ][ i ][ 0 ] - 1 ) )
                { 
                    Serial.print( F( "*" ) );
                    any_wrong_digitalPinToPCMSKbit_reports = true;
                }
                else Serial.print( F( " " ) );
                Serial.print( F( "    " ) );
            }
            else
            { 
                Serial.print( F( "              " ) );
            }
            Serial.print( F( "   " ) );
        }
        else Serial.print( F( "---                            " ) );
#endif
#if ( defined ( PCMSK ) && defined ( PCMSK0 ) ) || defined ( PCMSK1 )
        if ( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ 1 ] ) 
        { 
            Serial.print( F( "D" ) );
            Serial.print( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ 1 ] - 1 );
            if ( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ 1 ] < 101 ) Serial.print( F( " " ) );
            if ( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ 1 ] < 11 ) Serial.print( F( " " ) );
            print_analog_if_exists( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ 1 ] - 1 );
            if( digitalPinToPCICRbit( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ 1 ] - 1 ) != 1 )
            { 
                Serial.print( F( "!" ) );
                any_wrong_digitalPinToPCICRbit_reports = true;
            }
            else Serial.print( F( " " ) );
            if    ( i != digitalPinToPCMSKbit( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ 1 ] - 1 ) )
            { 
                Serial.print( F( "*" ) );
                any_wrong_digitalPinToPCMSKbit_reports = true;
            }
            else Serial.print( F( " " ) );
            if ( ( bool )( Isrspec[ 1 ].mask_by_PCMSK_of_valid_devices & 1<<i ) )
            {
                pinMode( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ 1 ] - 1, OUTPUT );
                digitalWrite( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ 1 ] - 1, HIGH ); //This places the level on the DHT devices that they need to be prepared to imminently and immediately send their data, assuming an adequate rest period
                Serial.print( F( "DHT" ) );
            }
            else
            {
                Serial.print( F( "   " ) );
                if( !pin_in_protected_arrays( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ 1 ] - 1 ) ) pinMode( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ 1 ] - 1, INPUT );
            }
            if ( PCINT_pins_by_PCMSK_and_ISR[ 1 ][ i ][ 1 ] ) 
            { 
                Serial.print( F( " and D" ) );
                Serial.print( PCINT_pins_by_PCMSK_and_ISR[ 1 ][ i ][ 1 ] - 1 );
                if ( PCINT_pins_by_PCMSK_and_ISR[ 1 ][ i ][ 1 ] < 101 ) Serial.print( F( " " ) );
                if ( PCINT_pins_by_PCMSK_and_ISR[ 1 ][ i ][ 1 ] < 11 ) Serial.print( F( " " ) );
                print_analog_if_exists( PCINT_pins_by_PCMSK_and_ISR[ 1 ][ i ][ 1 ] - 1 );
                if( digitalPinToPCICRbit( PCINT_pins_by_PCMSK_and_ISR[ 1 ][ i ][ 1 ] - 1 ) != 1 )
                { 
                    Serial.print( F( "!" ) );
                    any_wrong_digitalPinToPCICRbit_reports = true;
                }
                else Serial.print( F( " " ) );
                if    ( i != digitalPinToPCICRbit( PCINT_pins_by_PCMSK_and_ISR[ 1 ][ i ][ 1 ] - 1 ) )
                { 
                    Serial.print( F( "*" ) );
                    any_wrong_digitalPinToPCMSKbit_reports = true;
                }
                else Serial.print( F( " " ) );
                Serial.print( F( "    " ) );
            }
            else
                    Serial.print( F( "              " ) );
            Serial.print( F( "      " ) );
        }
        else Serial.print( F( "---                               " ) );
#endif
#ifdef PCMSK2
        if ( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ 2 ] ) 
        { 
            Serial.print( F( "D" ) );
            Serial.print( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ 2 ] - 1 );
            if ( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ 2 ] < 101 ) Serial.print( F( " " ) );
            if ( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ 2 ] < 11 ) Serial.print( F( " " ) );
            print_analog_if_exists( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ 2 ] - 1 );
            if( digitalPinToPCICRbit( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ 2 ] - 1 ) != 2 )
            { 
                Serial.print( F( "!" ) );
                any_wrong_digitalPinToPCICRbit_reports = true;
            }
            else Serial.print( F( " " ) );
            if    ( i != digitalPinToPCMSKbit( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ 2 ] - 1 ) )
            { 
                Serial.print( F( "*" ) );
                any_wrong_digitalPinToPCMSKbit_reports = true;
            }
            else Serial.print( F( " " ) );
            if ( ( bool )( Isrspec[ 2 ].mask_by_PCMSK_of_valid_devices & 1<<i ) )
            {
                pinMode( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ 2 ] - 1, OUTPUT );
                digitalWrite( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ 2 ] - 1, HIGH ); //This places the level on the DHT devices that they need to be prepared to imminently and immediately send their data, assuming an adequate rest period
                Serial.print( F( "DHT" ) );
            }
            else
            {
                Serial.print( F( "   " ) );
                if( !pin_in_protected_arrays( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ 2 ] - 1 ) ) pinMode( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ 2 ] - 1, INPUT );
            }
            if ( PCINT_pins_by_PCMSK_and_ISR[ 1 ][ i ][ 2 ] ) 
            { 
                Serial.print( F( " and D" ) );
                Serial.print( PCINT_pins_by_PCMSK_and_ISR[ 1 ][ i ][ 2 ] - 1 );
                if ( PCINT_pins_by_PCMSK_and_ISR[ 1 ][ i ][ 2 ] < 101 ) Serial.print( F( " " ) );
                if ( PCINT_pins_by_PCMSK_and_ISR[ 1 ][ i ][ 2 ] < 11 ) Serial.print( F( " " ) );
                print_analog_if_exists( PCINT_pins_by_PCMSK_and_ISR[ 1 ][ i ][ 2 ] - 1 );
                if( digitalPinToPCICRbit( PCINT_pins_by_PCMSK_and_ISR[ 1 ][ i ][ 2 ] - 1 ) != 2 )
                { 
                    Serial.print( F( "!" ) );
                    any_wrong_digitalPinToPCICRbit_reports = true;
                }
                else Serial.print( F( " " ) );
                if    ( i != digitalPinToPCICRbit( PCINT_pins_by_PCMSK_and_ISR[ 1 ][ i ][ 2 ] - 1 ) )
                { 
                    Serial.print( F( "*" ) );
                    any_wrong_digitalPinToPCMSKbit_reports = true;
                }
                else Serial.print( F( " " ) );
                Serial.print( F( "    " ) );
            }
            else
                    Serial.print( F( "              " ) );
            Serial.print( F( "   " ) );
        }
        else Serial.print( F( "---                               " ) );
#endif
#ifdef PCMSK3
        if ( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ 3 ] != NULL ) 
        { 
            Serial.print( F( "D" ) );
            Serial.print( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ 3 ] - 1 );
            if ( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ 3 ] < 101 ) Serial.print( F( " " ) );
            if ( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ 3 ] < 11 ) Serial.print( F( " " ) );
            print_analog_if_exists( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ 3 ]-1 );
            if( digitalPinToPCICRbit( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ 3 ] - 1 ) != 3 )
            { 
                Serial.print( F( "!" ) );
                any_wrong_digitalPinToPCICRbit_reports = true;
            }
            else Serial.print( F( " " ) );
            if    ( i != digitalPinToPCMSKbit( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ 3 ] - 1 ) )
            { 
                Serial.print( F( "*" ) );
                any_wrong_digitalPinToPCMSKbit_reports = true;
            }
            else Serial.print( F( " " ) );
            if ( ( bool )( Isrspec[ 3 ].mask_by_PCMSK_of_valid_devices & 1<<i ) )
            {
                pinMode( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ 3 ] - 1, OUTPUT );
                digitalWrite( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ 3 ] - 1, HIGH ); //This places the level on the DHT devices that they need to be prepared to imminently and immediately send their data, assuming an adequate rest period
                Serial.print( F( "DHT" ) );
            }
            else
            {
                Serial.print( F( "   " ) );
                if( !pin_in_protected_arrays( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ 3 ] - 1 ) ) pinMode( PCINT_pins_by_PCMSK_and_ISR[ 0 ][ i ][ 3 ] - 1, INPUT );
            }
            if ( PCINT_pins_by_PCMSK_and_ISR[ 1 ][ i ][ 3 ] != NULL ) 
            { 
                Serial.print( F( " and D" ) );
                Serial.print( PCINT_pins_by_PCMSK_and_ISR[ 1 ][ i ][ 3 ] - 1 );
                if ( PCINT_pins_by_PCMSK_and_ISR[ 1 ][ i ][ 3 ] < 101 ) Serial.print( F( " " ) );
                if ( PCINT_pins_by_PCMSK_and_ISR[ 1 ][ i ][ 3 ] < 11 ) Serial.print( F( " " ) );
                print_analog_if_exists( PCINT_pins_by_PCMSK_and_ISR[ 1 ][ i ][ 3 ] - 1 );
                if( digitalPinToPCICRbit( PCINT_pins_by_PCMSK_and_ISR[ 1 ][ i ][ 3 ] - 1 ) != 3 )
                { 
                    Serial.print( F( "!" ) );
                    any_wrong_digitalPinToPCICRbit_reports = true;
                }
                else Serial.print( F( " " ) );
                if    ( i != digitalPinToPCICRbit( PCINT_pins_by_PCMSK_and_ISR[ 1 ][ i ][ 3 ] - 1 ) )
                { 
                    Serial.print( F( "*" ) );
                    any_wrong_digitalPinToPCMSKbit_reports = true;
                }
                else Serial.print( F( " " ) );
                Serial.print( F( "    " ) );
            }
            else
                Serial.print( F( "   " ) );
        }
        else Serial.print( F( "---" ) );
#endif
    }
    Serial.println(); 
    Serial.println(); 
    if ( any_wrong_digitalPinToPCICRbit_reports )
    { 
        Serial.print( F( "! = digitalPinToPCICRbit() function reports the wrong PCICR for this pin.  This software product will work around it; other software will likely not correct the report error" ) );
        Serial.println(); 
    }
    if ( any_wrong_digitalPinToPCMSKbit_reports )
    { 
        Serial.print( F( "* = digitalPinToPCMSKbit() function reports the wrong PCMSK bit for this pin.  This software product will work around it; other software will likely not correct the report error" ) );
        Serial.println(); 
    }
#ifdef PCMSK4
    Serial.println( F( "Also note that at least one other ISR is available.  Modify this sketch to expand to explore it" ) );
#endif
    Serial.println(); 
    Serial.flush();
    Serial.end();
//WE FINALLY KNOW HOW MANY DHT DEVICES SERVED BY ISR THERE ARE.  MAKE AN ARRAY OF THEM WITH A XREF ARRAY
    for( u8 i = 0; i < number_of_ISRs; i++ )//free previous malloc of isrspec and refill isrspec_addr0 - 3 and reset Devspec.my_isrspec_addr
        if ( Isrspec[ i ].mask_by_PCMSK_of_valid_devices )
        {
            Isrxref->ISR_xref[ number_of_populated_isrs++ ] = i;
        }
    return ( true );
}// EDITOR IS WRONG ABOUT SHOWING THIS B/C IT DOESN'T KNOW OUTCOME OF IFDEFs


void setup() {
#ifndef OCR0A
    Serial.begin( _baud_rate_ ); //This speed is very dependent on the host's ability
    Serial.setTimeout( 10 ); //
#ifdef __LGT8FX8E__
    delay( 10000 );//Needed for this board for Serial communications to be reliable
#endif
    while ( !Serial ); // wait for serial port to connect. Needed for Leonardo's native USB
    Serial.println();
    Serial.print( F( "A necessary feature is not available: Timer0's A comparison register." ) );
    Serial.println();
    Serial.print( F( "This sketch will end." ) );
    Serial.println();
    Serial.flush();
    Serial.end();
    delay( 100000 );
    return ;
#endif
    if( ( ( TIMSK0 & 2 ) || TCCR0A != 3 ) && OCR0A != 0xA1 )
    {
        Serial.begin( _baud_rate_ ); //This speed is very dependent on the host's ability
        Serial.setTimeout( 10 ); //
        while ( !Serial ); // wait for serial port to connect. Needed for Leonardo's native USB
        Serial.println();
        Serial.println( F( "Judging from the contents of Timer0's A Output Compare Match registers, some other process is using the comparison feature this sketch is designed for" ) );
        Serial.println( F( "probably due to library changes since this sketch was published." ) );
        Serial.println( F( "Rather than disable anything else, this sketch will end as it is not sophisticated enough to play well with other processes using Match B" ) );
        Serial.flush();
        Serial.end();
        return ;
    }
    
#ifdef TIMER0_COMPA_vect
OCR0A = 0xA1; //To enable the 1 mSec off-phase ( compare ) interrupt
TIFR0 &= 0xFD; // to avoid an immediate interrupt occurring.  Clear this like this before expecting full first cycle
#endif
    Serial.begin( _baud_rate_ ); //This speed is very dependent on the host's ability
    Serial.setTimeout( 10 ); //
    while ( !Serial ); // wait for serial port to connect. Needed for Leonardo's native USB
    Serial.println();
    Serial.println();
    Serial.print( F( "Arduino DHTs on Interrupts Unleashed Sketch" ) );
    Serial.println();
    Serial.println();
    Serial.print( F( "This sketch will display the numbers of all digital pins with the ports and port masks" ) );
    Serial.println();
    Serial.print( F( "for them, detect and display all DHT devices connected (even those on pins not" ) );
    Serial.println();
    Serial.print( F( "supporting Pin Change Interrupts), and detect the existence of all Pin Change Interrupts" ) );
    Serial.println();
    Serial.print( F( "supported by the microcontroller board and display them for you." ) );
    Serial.println();
    Serial.println();
    Serial.print( F( "IT LEARNS THE INTERRUPT DETAIL BY TOGGLING PINS, so all pins must be free to toggle for" ) );
    Serial.println();
    Serial.print( F( "the results shown to be correct." ) );
    Serial.println();
    Serial.println();
    Serial.print( F( "If you need to, you may protect pins from being tested for devices by listing them in one" ) );
    Serial.println();
    Serial.print( F( "of the two protected pin arrays.  The built-in LED renders its pin useless for DHT use," ) );
    Serial.println();
    Serial.print( F( "so that pin is included in the list of protected pins by default and is given an alternate" ) );
    Serial.println();
    Serial.print( F( "function, if you need it, of being high during the duration of the device detection process." ) );
    Serial.println();
    Serial.print( F( "The intent is so it can be used to control signal-gating circuitry of your design and" ) );
    Serial.println();
    Serial.print( F( "construction to effectively disconnect pin signals and protect driven devices from the" ) );
    Serial.println();
    Serial.print( F( "extraneous toggling occurring during device detection." ) );
    Serial.println();
    Serial.println();
    Serial.print( F( "If you see nonsense characters displayed associated with the detection process, please" ) );
    Serial.println();
    Serial.print( F( "take the time now to add your board's serial communication pins to one of these" ) );
    Serial.println();
    Serial.print( F( "protecting arrays if you'll be using pins for serial communications." ) );
    Serial.println();
    Serial.flush();
    Serial.end();
//    unsigned short wincheck = resistor_between_LED_BUILTIN_and_PIN_A0();
    build_from_nothing();
    delay( 2000 );
    number_of_ports_with_functioning_DHT_devices_and_serviced_by_ISR = 0;    //This tells reset_ISR_findings_and_reprobe() that we need this variable re-valued to know how large the ports_with_DHT_devices... array must be initialized for
    reset_ISR_findings_and_reprobe ( false ); //uses number_of_ports_with_functioning_DHT_devices_and_serviced_by_ISR to build ptr_to_portspecs_stack
    TIMSK0 |= 2;  //enables the compare value for match A , = 1 go back to normal.  First run through
/*
Serial.flush();
Serial.end();
Serial.begin( _baud_rate_ ); //This speed is very dependent on the host's ability
Serial.setTimeout( 10 ); //
while ( !Serial );
Serial.flush();
Serial.end();
*/
Serial.begin( _baud_rate_ ); //This speed is very dependent on the host's ability
Serial.setTimeout( 10 ); //
while ( !Serial ) ; // wait for serial port to connect. Needed for Leonardo's native USB
#ifndef __LGT8FX8E__
    if( !( ( ( long unsigned int )ports_string_in_heap_array - ( long unsigned int )Devspec ) / sizeof( DEVSPEC ) ) )
    {
        Serial.println( F( "No DHT devices were detected, so the following statements are null and void:" ) );
    }
    Serial.print( F( "Factory sketch functions: enter the letter A or a number between 0 and " ) );
    Serial.print( ( ( ( long unsigned int )ports_string_in_heap_array - ( long unsigned int )Devspec ) / sizeof( DEVSPEC ) ) - 1 );
    Serial.print( F( " with your entire" ) );
    Serial.println();
    Serial.print( F( "entry enclosed between these two characters: < and >.  Entering the letter A so enclosed" ) );
    Serial.println();
    Serial.print( F( "will list all DHT devices each with its last " ) );
    Serial.print( confidence_depth );
    Serial.print( F( " values obtained.  Entering the index" ) );
    Serial.println();
    Serial.print( F( "number of any selected device will do the same for the one device only.  Reading errors" ) );
    Serial.println();
    Serial.print( F( "greater than " ) );
    Serial.print( alert_beyond_this_number_of_consecutive_errs );
    Serial.print( F( " consecutively are displayed asynchronously by void loop() as" ) );
    Serial.println();
    Serial.print( F( "they happen." ) );
    Serial.println();
    Serial.flush();
#else
    Serial.println( F( "End of sketch ability in this board" ) );
#endif
}

const byte numChars = 32;
char receivedChars[ numChars ];   // an array to store the received data
boolean newData = false;
void recvWithStartEndMarkers() //COURTESY Robin2 ON http://forum.arduino.cc/index.php?topic=396450
{
    static boolean recvInProgress = false;
    static byte ndx = 0;
    char startMarker = '<';
    char endMarker = '>';
    char rc;
 
    while (Serial.available() > 0 && newData == false) {
        rc = Serial.read();

        if (recvInProgress == true) {
            if (rc != endMarker) {
                receivedChars[ndx] = rc;
                ndx++;
                if (ndx >= numChars) {
                    ndx = numChars - 1;
                }
            }
            else {
                receivedChars[ndx] = '\0'; // terminate the string
                recvInProgress = false;
                ndx = 0;
                newData = true;
            }
        }

        else if (rc == startMarker) {
            recvInProgress = true;
        }
    }
}

void showNewData() //COURTESY Robin2 ON http://forum.arduino.cc/index.php?topic=396450
{
    if( newData )
    {
        DEVSPEC* this_Devspec_address;
        u8 tmp_sandbox;
        u8 filled_vals;
        u8 ilvr;
        if( ( receivedChars[ 0 ] == 'a' ) || ( receivedChars[ 0 ] == 'A' ) )
        {
            for( u8 devspec_index = 0; devspec_index < ( ( long unsigned int )ports_string_in_heap_array - ( long unsigned int )Devspec ) / sizeof( DEVSPEC ); devspec_index++ )
            {
                this_Devspec_address = &Devspec[ devspec_index ];
                tmp_sandbox;
                filled_vals = 0;
                ilvr = this_Devspec_address->index_of_next_valid_readings_sets;
                Serial.print( F( "At location #" ) );
                if( devspec_index < 10 ) Serial.print( F( " " ) );
                Serial.print( devspec_index );
                if( this_Devspec_address->Dpin < 10 ) Serial.print( F( " " ) );
                Serial.print( F( " is pin D" ) );
                Serial.print( this_Devspec_address->Dpin );
                Serial.print( F( " " ) );
                print_analog_if_exists( this_Devspec_address->Dpin );
                Serial.print( F( "(DHT" ) );
                if( this_Devspec_address->devprot_index ) Serial.print( F( "22" ) );
                else Serial.print( F( "11" ) );
                Serial.print( F( "): " ) );
                Serial.flush();
                for( signed char ij = confidence_depth - 1; ij >= 0 ;ij-- )
                {
                    if( this_Devspec_address->last_valid_data_bytes_from_dht_device[ ( u8 )( ( sizeof( this_Devspec_address->last_valid_data_bytes_from_dht_device ) / confidence_depth ) * ( ( ij + ilvr ) % confidence_depth ) ) ] < 10 ) Serial.print( F( " " ) );
                    Serial.print( this_Devspec_address->last_valid_data_bytes_from_dht_device[ ( u8 )( ( sizeof( this_Devspec_address->last_valid_data_bytes_from_dht_device ) / confidence_depth ) * ( ( ij + ilvr ) % confidence_depth ) ) ] );
                    Serial.print( F( "." ) );
                    Serial.print( this_Devspec_address->last_valid_data_bytes_from_dht_device[ ( u8 )( 1 + ( ( sizeof( this_Devspec_address->last_valid_data_bytes_from_dht_device ) / confidence_depth ) * ( ( ij + ilvr ) % confidence_depth ) ) ) ] );
                    Serial.print( F( "% " ) );
            
                    for( u8 ik = filled_vals = 0; ik < confidence_depth; ik++ )
                        if( this_Devspec_address->last_valid_data_bytes_from_dht_device[ ( ( sizeof( this_Devspec_address->last_valid_data_bytes_from_dht_device ) / confidence_depth ) * ik ) ] )
                            filled_vals++;
                    for( u8 ik = tmp_sandbox = 0; ik < confidence_depth; ik++ )
                        if( this_Devspec_address->last_valid_data_bytes_from_dht_device[ 2 + ( ( sizeof( this_Devspec_address->last_valid_data_bytes_from_dht_device ) / confidence_depth ) * ik ) ] & 0x80 ) tmp_sandbox++;
                    if( ( this_Devspec_address->last_valid_data_bytes_from_dht_device[ 2 + ( ( sizeof( this_Devspec_address->last_valid_data_bytes_from_dht_device ) / confidence_depth ) * ( ( ij + ilvr ) % confidence_depth ) ) ] & 0x7F ) < 10 ) Serial.print( F( " " ) );
                    if( tmp_sandbox > ( filled_vals >> 1 ) )
                        Serial.print( F( "-" ) );
            
                    Serial.print( this_Devspec_address->last_valid_data_bytes_from_dht_device[ 2 + ( ( sizeof( this_Devspec_address->last_valid_data_bytes_from_dht_device ) / confidence_depth ) * ( ( ij + ilvr ) % confidence_depth ) ) ] & 0x7F );
                    Serial.print( F( "." ) );
                    Serial.print( this_Devspec_address->last_valid_data_bytes_from_dht_device[ 3 + ( ( sizeof( this_Devspec_address->last_valid_data_bytes_from_dht_device ) / confidence_depth ) * ( ( ij + ilvr ) % confidence_depth ) ) ] );
            
                    Serial.print( F( "C " ) );
                }
                Serial.print( F( "age in seconds = " ) );
                if( ( ( float )( ( unsigned long )( millis() - this_Devspec_address->timestamp_of_pin_valid_data_millis ) ) / 1000 ) < 10 ) Serial.print( F( " " ) );
                Serial.print( ( float )( ( unsigned long )( millis() - this_Devspec_address->timestamp_of_pin_valid_data_millis ) ) / 1000 );
                Serial.print( F( " " ) );
                if( ( ( float )( ( unsigned long )( millis() - this_Devspec_address->timestamp_of_pin_last_attempted_device_read_millis ) ) / 1000 ) < 10 ) Serial.print( F( " " ) );
                Serial.print( ( float )( ( unsigned long )( millis() - this_Devspec_address->timestamp_of_pin_last_attempted_device_read_millis ) ) / 1000 );
                Serial.print( F( " = last_attempted_read seconds ago. remaining rest: " ) );
                Serial.print( this_Devspec_address->device_busy_resting_this_more_millis );
                Serial.print( F( "mS " ) );
                Serial.println();
                Serial.flush();
            }
        }
        else if( ( receivedChars[ 0 ] == 'd' ) || ( receivedChars[ 0 ] == 'D' ) )
        {
            ;
        }
        else if( atoi( receivedChars ) < ( ( long unsigned int )ports_string_in_heap_array - ( long unsigned int )Devspec ) / sizeof( DEVSPEC ) )
        {
            this_Devspec_address = &Devspec[ atoi( receivedChars ) ];
            tmp_sandbox;
            filled_vals = 0;
            ilvr = this_Devspec_address->index_of_next_valid_readings_sets;
            Serial.print( F( "At location #" ) );
            if( atoi( receivedChars ) < 10 ) Serial.print( F( " " ) );
            Serial.print( atoi( receivedChars ) );
            if( this_Devspec_address->Dpin < 10 ) Serial.print( F( " " ) );
            Serial.print( F( " is pin D" ) );
            Serial.print( this_Devspec_address->Dpin );
            Serial.print( F( " " ) );
            print_analog_if_exists( this_Devspec_address->Dpin );
            Serial.print( F( "(DHT" ) );
            if( this_Devspec_address->devprot_index ) Serial.print( F( "22" ) );
            else Serial.print( F( "11" ) );
            Serial.print( F( "): " ) );
            for( signed char ij = confidence_depth - 1; ij >= 0 ;ij-- )
            {
                if( this_Devspec_address->last_valid_data_bytes_from_dht_device[ ( u8 )( ( sizeof( this_Devspec_address->last_valid_data_bytes_from_dht_device ) / confidence_depth ) * ( ( ij + ilvr ) % confidence_depth ) ) ] < 10 ) Serial.print( F( " " ) );
                Serial.print( this_Devspec_address->last_valid_data_bytes_from_dht_device[ ( u8 )( ( sizeof( this_Devspec_address->last_valid_data_bytes_from_dht_device ) / confidence_depth ) * ( ( ij + ilvr ) % confidence_depth ) ) ] );
                Serial.print( F( "." ) );
                Serial.print( this_Devspec_address->last_valid_data_bytes_from_dht_device[ ( u8 )( 1 + ( ( sizeof( this_Devspec_address->last_valid_data_bytes_from_dht_device ) / confidence_depth ) * ( ( ij + ilvr ) % confidence_depth ) ) ) ] );
                Serial.print( F( "% " ) );
        
                for(u8 ik = filled_vals = 0; ik < confidence_depth; ik++)
                    if( this_Devspec_address->last_valid_data_bytes_from_dht_device[ ( ( sizeof( this_Devspec_address->last_valid_data_bytes_from_dht_device ) / confidence_depth ) * ik ) ] )
                        filled_vals++;
                for(u8 ik = tmp_sandbox = 0; ik < confidence_depth; ik++)
                    if( this_Devspec_address->last_valid_data_bytes_from_dht_device[ 2 + ( ( sizeof( this_Devspec_address->last_valid_data_bytes_from_dht_device ) / confidence_depth ) * ik ) ] & 0x80 ) tmp_sandbox++;
                if( tmp_sandbox > ( filled_vals >> 1 ) )
                    Serial.print( F( "-" ) );
        
                if( ( this_Devspec_address->last_valid_data_bytes_from_dht_device[ 2 + ( ( sizeof( this_Devspec_address->last_valid_data_bytes_from_dht_device ) / confidence_depth ) * ( ( ij + ilvr ) % confidence_depth ) ) ] & 0x7F ) < 10 ) Serial.print( F( " " ) );
                Serial.print( this_Devspec_address->last_valid_data_bytes_from_dht_device[ 2 + ( ( sizeof( this_Devspec_address->last_valid_data_bytes_from_dht_device ) / confidence_depth ) * ( ( ij + ilvr ) % confidence_depth ) ) ] & 0x7F );
                Serial.print( F( "." ) );
                Serial.print( this_Devspec_address->last_valid_data_bytes_from_dht_device[ 3 + ( ( sizeof( this_Devspec_address->last_valid_data_bytes_from_dht_device ) / confidence_depth ) * ( ( ij + ilvr ) % confidence_depth ) ) ] );
        
                Serial.print( F( "C " ) );
            }
            Serial.print( F( "age in seconds = " ) );
            if( ( ( float )( ( unsigned long )( millis() - this_Devspec_address->timestamp_of_pin_valid_data_millis ) ) / 1000 ) < 10 ) Serial.print( F( " " ) );
            Serial.print( ( float )( ( unsigned long )( millis() - this_Devspec_address->timestamp_of_pin_valid_data_millis ) ) / 1000 );
            Serial.print( F( " " ) );
            if( ( ( float )( ( unsigned long )( millis() - this_Devspec_address->timestamp_of_pin_last_attempted_device_read_millis ) ) / 1000 ) < 10 ) Serial.print( F( " " ) );
            Serial.print( ( float )( ( unsigned long )( millis() - this_Devspec_address->timestamp_of_pin_last_attempted_device_read_millis ) ) / 1000 );
            Serial.print( F( " = last_attempted_read seconds ago. remaining rest: " ) );
            Serial.print( this_Devspec_address->device_busy_resting_this_more_millis );
            Serial.print( F( "mS " ) );
            Serial.println();
        }
        else
        {
            ;//We could print out some stats
        }
        newData = false;
    }
}

void loop() 
{
    Serial.flush();
    recvWithStartEndMarkers();//COURTESY Robin2 ON http://forum.arduino.cc/index.php?topic=396450
    showNewData();//COURTESY Robin2 ON http://forum.arduino.cc/index.php?topic=396450
    for( u8 devspec_index = 0; devspec_index < ( ( long unsigned int )ports_string_in_heap_array - ( long unsigned int )Devspec ) / sizeof( DEVSPEC ); devspec_index++ )
    {
        DEVSPEC* this_Devspec_address = &Devspec[ devspec_index ];
        if( this_Devspec_address->consecutive_read_failures_mode0 >= alert_beyond_this_number_of_consecutive_errs \
            || this_Devspec_address->consecutive_read_failures_mode1 >= alert_beyond_this_number_of_consecutive_errs \
            || this_Devspec_address->consecutive_read_failures_mode2 >= alert_beyond_this_number_of_consecutive_errs \
            || this_Devspec_address->consecutive_read_failures_mode3 >= alert_beyond_this_number_of_consecutive_errs \
            || this_Devspec_address->consecutive_read_failures_mode4 >= alert_beyond_this_number_of_consecutive_errs )
        {
            Serial.print( F( "At location #" ) );
            if( devspec_index < 10 ) Serial.print( F( " " ) );
            Serial.print( devspec_index );
            if( this_Devspec_address->Dpin < 10 ) Serial.print( F( " " ) );
            Serial.print( F( " is pin D" ) );
            Serial.print( this_Devspec_address->Dpin );
            Serial.print( F( " " ) );
            print_analog_if_exists( this_Devspec_address->Dpin );
            Serial.print( F( " (DHT" ) );
            if( this_Devspec_address->devprot_index ) Serial.print( F( "22" ) );
            else Serial.print( F( "11" ) );
            Serial.print( F( ") failures " ) );
            Serial.print( this_Devspec_address->consecutive_read_failures_mode0 );
            Serial.print( F( " " ) );
            Serial.print( this_Devspec_address->consecutive_read_failures_mode1 );
            Serial.print( F( " " ) );
            Serial.print( this_Devspec_address->consecutive_read_failures_mode2 );
            Serial.print( F( " " ) );
            Serial.print( this_Devspec_address->consecutive_read_failures_mode3 );
            Serial.print( F( " " ) );
            Serial.print( this_Devspec_address->consecutive_read_failures_mode4 );
            Serial.println();
        }
    }
    if( ( strlen( ports_string_in_heap_array ) > 26 ) || ( ( u8 )ports_string_in_heap_array[ strlen( ports_string_in_heap_array ) + 1 ] != 255 ) )
    {
        Serial.print( F( "Heap overwrite failure " ) );
        Serial.println();
        delay( 30000 );
    }
    Serial.flush();
    delay( 200 );//add 400 for loop execution time, gives us about 600 for loop interval time
}
