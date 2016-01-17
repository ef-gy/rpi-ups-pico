/**\file
 * \brief Arduino proof-of-concept picod.
 *
 * This file implements a proof-of-concept Arduino version of picod. To use it,
 * hook up pin #12 on your Arduino to GPIO pin #22 of your PIco and the IO pin
 * row's GROUND to any of the GROUND pins closeby.
 *
 * Then, once you hook up the +5V and/or Vin of your Arduino to the PIco, you
 * should be able to emulate the pulse wave the PIco needs to think it can
 * charge. Unfortunately the Arduino Uno only provides about 100 mA per block,
 * which is not enough to actually charge the PIco's battery and will result in
 * the firmware winking out and getting confused.
 *
 * It does work well enough to power the Arduino if the battery was charged
 * beforehand. Yay for simple signalling :).
 *
 * \see Source Code Repository: https://github.com/ef-gy/rpi-ups-pico
 * \see Documentation: https://ef.gy/documentation/rpi-ups-pico
 * \see Hardware: http://pimodules.com/_pdf/_pico/UPS_PIco_BL_FSSD_V1.0.pdf
 * \see Hardware Vendor: http://pimodules.com/
 * \see Licence Terms: https://github.com/ef-gy/rpi-ups-pico/blob/master/LICENSE
 */

/**\brief Pulse train pin
 *
 * This is the pin to create the pulse train on.
 */
int target = 12;

/**\brief Set up output pin.
 *
 * Declares the output pin as such.
 */
void setup() {                
  pinMode(target, OUTPUT);     
}

/**\brief Create pulse train.
 *
 * Our loop function is exceedingly simple, as all we have to do is create the
 * pulse train on our output pin for the PIco to think we're a legit Raspberry
 * Pi. That was easy...
 */
void loop() {
  digitalWrite(target, HIGH);
  delay(250);
  digitalWrite(target, LOW);
  delay(250);
}
