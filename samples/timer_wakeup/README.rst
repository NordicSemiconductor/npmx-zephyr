.. _timer_wake_up_sample:

Wake-up Timer
#############

.. contents::
   :local:
   :depth: 2

The Wake-up Timer sample demonstrates how to use the PMIC TIMER peripheral and the SHIPHLD button with npmx drivers.

Requirements
************

The sample supports the following development kits:

.. table-from-sample-yaml::

The sample also requires an nPM1300 EK.

Overview
********

This sample uses the Timer and SHIP drivers to do the following:

  * Configure the SHIPHLD button.
  * Register a handler to be called when the SHIPHLD button is pressed.
  * Configure the TIMER peripheral in the Wake-up Timer mode.

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
     * - SDA
       - P1.02
       - P0.26
     * - SCL
       - P1.03
       - P0.27
     * - VOUT2 & GND
       - External supply (P21)
       - External supply (P21)

#. Make the following connections on the nPM1300 EK:

   * Connect a USB power supply to the **J3** connector.
   * Connect a suitable battery to the **J2** connector.
   * On the **P18** pin header, connect **VOUT2** and **VDDIO** pins with a jumper.
   * On the **P2** pin header, connect **VBAT** and **VBATIN** pins with a jumper.
   * On the **P17** pin header, connect all LEDs with jumpers.
   * On the **P13** pin header, connect **RSET1** and **GND** pins with a jumper.
   * On the **P14** pin header, connect **RSET2** and **VSET2** pins with a jumper.

#. Make the following connections on the chosen DK:

   * Set the **SW9** nRF power source switch to **VDD**.
   * Set the **SW10** VEXT → nRF switch to **ON**.

Configuration
*************

|config|

Building and running
********************

.. |sample path| replace:: :file:`samples/timer_wakeup`

.. include:: /includes/build_and_run.txt

Testing
=======

|test_sample|

#. |connect_kit|
#. |connect_terminal|
#. Press the SW1 (SHPHLD) button to enter the hibernate mode.
#. Wait for ~8 seconds until the device wakes up.

When the PMIC wakes up, the LED will turn on.

Dependencies
************

This sample uses drivers from npmx.
