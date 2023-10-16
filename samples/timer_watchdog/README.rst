.. _timer_watchdog_sample:

Timer Watchdog
##############

.. contents::
   :local:
   :depth: 2

The Watchdog Timer sample demonstrates how to configure a PMIC TIMER peripheral in the Watchdog Timer mode with the use of npmx drivers.

Requirements
************

The sample supports the following development kits:

.. table-from-sample-yaml::

The sample also requires an nPM1300 EK.

Overview
********

This sample uses the Timer driver to do the following:

  * Enter and configure the TIMER peripheral in the Timer Watchdog mode.
  * Register a handler to be called when the WATCHDOG event occurs.
  * Kick the watchdog in a loop until any keyboard button is pressed.

The TIMER peripheral is configured to generate a watchdog warning interrupt ~500 ms after any keyboard button is pressed.

Wiring
******

#. Connect the TWI interface between the chosen DK and the nPM1300 EK as in the following table:

   .. list-table:: nPM1300 EK connections.
     :widths: 25 25 25
     :header-rows: 1

     * - nPM1300 EK pins
       - nRF5340 DK pins
       - nRF52840 DK pins
     * - GPIO0
       - P1.10
       - P1.10
     * - GPIO1
       - P1.11
       - P1.11
     * - SDA
       - P1.02
       - P0.26
     * - SCL
       - P1.03
       - P0.27
     * - VOUT2 & GND
       - External supply (P21)
       - External supply (P21)

#. Make the following connections on the chosen DK:

   * Set the **SW9** nRF power source switch to **VDD**.
   * Set the **SW10** VEXT → nRF switch to **ON**.

#. Make the following connections on the nPM1300 EK:

   .. include:: /includes/npm1300_ek_connections.txt

Configuration
*************

Check and configure the following sample-specific Kconfig options to specify which testcase you want to run:

.. _CONFIG_TESTCASE_WATCHDOG_MODE_WARNING:

CONFIG_TESTCASE_WATCHDOG_MODE_WARNING
  This sample configuration enables the warning mode for the watchdog.
  An interrupt is signaled on the **GPIO0** pin.
  16 ms after signaling, the **GPIO1** pin is set to 0.0 V.

.. _CONFIG_TESTCASE_WATCHDOG_MODE_RESET:

CONFIG_TESTCASE_WATCHDOG_MODE_RESET
  This sample configuration enables the reset mode for the watchdog.
  An interrupt is signaled on the **GPIO0** pin.
  The **GPIO1** pin is set to 0.0 V immediately, and the PMIC is restarted.

Building and running
********************

.. |sample path| replace:: :file:`samples/watchdog`

.. include:: /includes/build_and_run.txt

Testing
=======

|test_sample|

#. |connect_kit|
#. |connect_terminal|
#. Connect a logic analyzer first to the **GPIO0** pin and then to the **GPIO1** one, and observe the lines state.
#. Press any keyboard button to stop kicking the watchdog.

The watchdog callback should be generated after ~500 ms.
For `CONFIG_TESTCASE_WATCHDOG_MODE_WARNING`_, the **GPIO1** pin should be set to 0.0 V within 16 ms after signaling the interrupt on the **GPIO0** pin.
For `CONFIG_TESTCASE_WATCHDOG_MODE_RESET`_, the **GPIO1** pin should be set to 0.0 V immediately after signaling the interrupt on the **GPIO0** pin.

Dependencies
************

This sample uses drivers from npmx.
