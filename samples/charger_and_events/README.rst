.. _charger_and_events_sample:

CHARGER and Events
##################

.. contents::
   :local:
   :depth: 2

The CHARGER and Events sample demonstrates how to control a PMIC charger and receive events using npmx drivers.

Requirements
************

The sample supports the following development kits:

.. table-from-sample-yaml::

The sample also requires an nPM1300 EK.

Overview
********

This sample uses CHARGER and Events drivers to do the following:

* Set the charger's basic configuration.
* Register the events from the device.

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

Configuration options
=====================

Check and configure the following sample-specific Kconfig options:

.. CONFIG_CHARGING_CURRENT:

CONFIG_CHARGING_CURRENT
  This option changes the battery charging current.

.. CONFIG_TERMINATION_VOLTAGE:

CONFIG_TERMINATION_VOLTAGE
  This option changes the battery termination voltage.

.. CONFIG_BATTERY_VOLTAGE_THRESHOLD_1:

CONFIG_BATTERY_VOLTAGE_THRESHOLD_1
  This option changes the first battery threshold voltage to be detected.

.. CONFIG_BATTERY_VOLTAGE_THRESHOLD_2:

CONFIG_BATTERY_VOLTAGE_THRESHOLD_2
  This option changes the second battery threshold voltage to be detected.

Building and running
********************

.. |sample path| replace:: :file:`samples/charger_and_events`

.. include:: /includes/build_and_run.txt

Testing
=======

|test_sample|

#. |connect_kit|
#. |connect_terminal|
#. Connect and disconnect the battery and the USB-C cable to see various event messages.
   Battery charging status will also be printed.

Dependencies
************

This sample uses drivers from npmx.
