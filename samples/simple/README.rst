.. _simple_sample:

Simple
######

.. contents::
   :local:
   :depth: 2

The Simple sample demonstrates how to perform basic activities on CHARGER, ADC, LEDs and USB CC with npmx drivers.

Requirements
************

The sample supports the following development kits:

.. table-from-sample-yaml::

The sample also requires an nPM1300 EK.

Overview
********

This sample uses npmx drivers to do the following:

* Set the charger with charging current and termination voltage.
* Initialize and register USB CC events.
* Initialize LEDs.
* Enable ADC NTC measurements.

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

#. Make the following connections on the DK:

   * Set the **SW9** nRF power source switch to **VDD**.
   * Set the **SW10** VEXT → nRF switch to **ON**.

#. Make the following connections on the nPM1300 EK:

   .. include:: /includes/npm1300_ek_connections_with_battery.txt

Configuration
*************

|config|

Building and running
********************

.. |sample path| replace:: :file:`samples/simple`

.. include:: /includes/build_and_run.txt

Testing
=======

|test_sample|

#. |connect_kit|
#. |connect_terminal|
#. Check the terminal output to review messages about the power supply current.

Dependencies
************

This sample uses drivers from npmx.
