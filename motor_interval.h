// motor_interval.h
#pragma once

#include "scheduler.h"
#include "switch.h"
#include "mup_util.h"

#ifndef MQTT_MAX_PACKET_SIZE
#define MAX_MESSAGE_SIZE    512
#else
#define MAX_MESSAGE_SIZE    MQTT_MAX_PACKET_SIZE
#endif

namespace ustd {

    class MotorInterval {
    public:
        /*! MotorInterval implements a simple dc motor whose advance is controlled by a sensor
         * switch triggered by a notch wheel.
         *
         * The connection requires one digital ouput GPIO pin for the motor driver and one
         * digital input pin for the sensor switch.
         *
         * It is possible to start the motor and optionally specify how many intervals shall
         * the motor advance before automatically stopping. It is also possible to explicitely
         * stop the motor before the requested number of intervals will be reached. The current
         * interval will be completed before stopping.
         * During motion, it is also possible to change the number of intervals requested.
         *
         * A security mechanism performs a hard emergency stop if the sensor switch does not
         * report any activity during a specified timeout.
         */
        String MOTORINTERVAL_VERSION = "0.1.0";
        Scheduler* pSched;
        int tID;
        String name;
        uint8_t motorPort;
        uint8_t sensorPort;
        unsigned long sensorTimeout;
        unsigned maxIntervals = UINT_MAX;
        bool motorActiveLogic = false;
        bool sensorActiveLogic = false;
        bool state;
        unsigned long startTime = millis();
        unsigned long stopTime = 0;
        unsigned long lastTime = 0;
        unsigned target_interval = 0;
        unsigned current_interval = 0;
        String lastResult = "Not initialized";
        Switch intervalSensor;

        MotorInterval( String name, uint8_t motorPort, uint8_t sensorPort, unsigned long sensorTimeout, bool motorActiveLogic = false, bool sensorActiveLogic = false )
            : name( name ), motorPort( motorPort ), sensorPort( sensorPort ), sensorTimeout( sensorTimeout ), motorActiveLogic( motorActiveLogic ), sensorActiveLogic( sensorActiveLogic ), intervalSensor( name + ".sensor", sensorPort, ustd::Switch::Mode::Default, sensorActiveLogic ) {
            /*! Instantiate an Interval Motor
             * @param name              The name of the entity
             * @param motorPort         The pin identifier for the digital output GPIO port that controls the motor
             * @param sensorPort        The pin identifier for the digital input GPIO port connected to the swicth
             *                          triggered at each interval
             * @param sensorTimeout     The timeout in milliseconds for the motor to advance for one interval. If
             *                          no sensor input is received, the motor will perform an emergecy stop and
             *                          report an error
             * @param motorActiveLogic  If true, the motor logic is inverted (motor on on LOW). Default is false
             * @param sensorActiveLogic If true, the sensor logic is inverted. Default is false
             */
        }

        ~MotorInterval() {
        }

        void setSensorTimeout( unsigned long ms ) {
            /*! Change the sensor timeout
             * @param sensorTimeout The timeout in milliseconds for the motor to advance for one interval. If
             *                      no sensor input is received, the motor will perform an emergecy stop and
             *                      report an error
             */
            sensorTimeout = ms;
        }

        void setMaxIntervals( unsigned intervals ) {
            /*! Change the number of maximum intervals to advance on undetermined start
             * @param intervals The maximum number of intervals that the entity will advance on
             *                  an undetermined start command
             */
            maxIntervals = intervals;
        }

        void begin( Scheduler* _pSched ) {
            /*! Initialize the interval motor mupplet
             *
             * @param _pSched   Pointer to the application's scheduler
             */
            pSched = _pSched;
            lastResult = "OK";
            pinMode( motorPort, OUTPUT );
            pinMode( sensorPort, INPUT );

            stopMotor();
            tID = pSched->add( [this]() { this->loop(); }, name, 50000 );

            pSched->subscribe(
                tID, name + "/switch/#", [this]( String topic, String msg, String originator ) {
                    this->commandMsg( topic, msg, originator );
                }
            );

            pSched->subscribe(
                tID, name + ".sensor/switch/#", [this]( String topic, String msg, String originator ) {
                    this->sensorMsg( topic, msg, originator );
                }
            );

            intervalSensor.begin( _pSched );
            pSched->publish( name + "/switch/result", lastResult.c_str() );
        }

        void start( unsigned intervals = UINT_MAX ) {
            /*! Starts the motor
             * @param intervals Number of intervals before automatically stopping the motor.
             *                  Default is the maxIntervals settings.
             *
             * If the motor is already running, the number of intervals to advance is changed to the
             * specified value. By specifing a value equal or lesser than the vallue of the already
             * advanced intervals, the motor will stop as soon as the current interval has been
             * completed.
             */
            set( intervals );
        }

        void stop( bool hardstop = false ) {
            /*! Stops the motor when the current interval is finished
             *
             * @param hardstop  If true, the motor is stopped immediately without waiting for the current
             *                  interval to complete. In this case the reported number of intervals will
             *                  only include the last fully completed interval. If the motor is restarted
             *                  the next interval will be shorter since the motor starts at a intermediate
             *                  position. Default is false
             */
            if ( state && hardstop ) {
                // stop immediately
                stopMotor();
                publishState();
            } else if ( state ) {
                // stop at next finished interval
                target_interval = current_interval + 1;
                publishf( name + "/switch/requestedintervals", "%u", target_interval );
            }
        }

        void set( unsigned intervals ) {
            /*! Controls the motor
             * @param intervals Number of intervals before automatically stopping the motor.
             *
             * If the motor is already running, the number of intervals to advance is changed to the
             * specified value. By specifing a value equal or lesser than the vallue of the already
             * advanced intervals, the motor will stop as soon as the current interval has been
             * completed.
             */
            if ( state ) {
                if ( intervals ) {
                    target_interval = intervals == UINT_MAX ? maxIntervals : intervals;
                    publishf( name + "/switch/requestedintervals", "%u", target_interval );
                } else {
                    stop();
                }
            } else {
                if ( intervals ) {
                    target_interval = intervals == UINT_MAX ? maxIntervals : intervals;
                    current_interval = 0;
                    lastResult = "OK";
                    startMotor();
                    pSched->publish( name + "/switch/state", "on" );
                    publishf( name + "/switch/requestedintervals", "%u", target_interval );
                }
            }
        }

        void publishState() {
            pSched->publish( name + "/switch/state", state ? "on" : "off" );
            pSched->publish( name + "/switch/result", lastResult.c_str() );
            publishf( name + "/switch/requestedintervals", "%u", target_interval );
            publishf( name + "/switch/intervals", "%u", current_interval );
            publishf( name + "/switch/duration", "%lu", timeDiff( startTime, stopTime ) );
        }

        virtual void loop() {
            if ( state && timeDiff( lastTime, millis() ) > sensorTimeout ) {
                // sensor broken or movement stuck
                lastResult = "No feedback from motor sensor. Please check device.";
                stop( true );
            }
        }

    protected:
        void publishf( String topic, String format, ... ) {
            va_list args;
            va_start( args, format );
            vpublishf( topic, format.c_str(), args );
            va_end( args );
        }

        void publishf( String topic, const char* format, ... ) {
            va_list args;
            va_start( args, format );
            vpublishf( topic, format, args );
            va_end( args );
        }

        void vpublishf( String topic, const char* format, va_list args ) {
            char buffer[ MAX_MESSAGE_SIZE ];
            vsnprintf( buffer, MAX_MESSAGE_SIZE - 1, format, args );
            buffer[ MAX_MESSAGE_SIZE - 1 ] = 0;
            pSched->publish( topic, buffer );
        }

        void commandMsg( String topic, String msg, String originator ) {
            msg.toLowerCase();
            if ( topic == name + "/switch/set" ) {
                if ( msg == "on" ) {
                    start();
                } else if ( msg == "off" ) {
                    stop();
                } else {
                    int value = atoi( msg.c_str() );
                    if ( value > 0 ) {
                        start( value );
                    } else if ( value == 0 ) {
                        stop();
                    } else {
                        // negative value -> hard stop!
                        stop( true );
                    }
                }
            }
        };

        void sensorMsg( String topic, String msg, String originator ) {
            if ( topic == name + ".sensor/switch/state" ) {
                if ( msg == "off" ) {
                    lastTime = millis();
                    ++current_interval;
                    if ( current_interval >= target_interval ) {
                        stopMotor();
                        publishState();
                    }
                }
            }
        }

        void startMotor() {
            state = true;
            startTime = millis();
            stopTime = lastTime = startTime;
            if ( motorActiveLogic ) {
                digitalWrite( motorPort, true );
            } else {
                digitalWrite( motorPort, false );
            }
        }

        void stopMotor() {
            state = false;
            stopTime = millis();
            if ( !motorActiveLogic ) {
                digitalWrite( motorPort, true );
            } else {
                digitalWrite( motorPort, false );
            }
        }
    };

}