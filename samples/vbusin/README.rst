.. _vbusin_sample:

VBUSIN
######

.. contents::
   :local:
   :depth: 2

The VBUSIN sample demonstrates how to receive messages regarding a connected power supply with the use of npmx drivers.
These messages are based on the USB CC lines status.

Requirements
************

The sample supports the following development kits:

.. table-from-sample-yaml::

The sample also requires an nPM1300 EK.

Overview
********

This sample uses the VBUSIN driver to do the following:

* Set the current limit for the USB port.
* Register USB CC events.

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

#. Make the following connections on the chosen DK:

   * Set the **SW9** nRF power source switch to **VDD**.
   * Set the **SW10** VEXT → nRF switch to **ON**.

#. Make the following connections on the nPM1300 EK:

   .. include:: /includes/npm1300_ek_connections_with_battery.txt

Configuration
*************

|config|

Building and running
********************

.. |sample path| replace:: :file:`samples/vbusin`

.. include:: /includes/build_and_run.txt

Testing
=======

|test_sample|

#. |connect_kit|
#. |connect_terminal|
#. Connect the battery.
#. Connect and disconnect various USB power supplies to the **J3** connector to see messages about current capability of the power supply.

Dependencies
************

This sample uses drivers from npmx.
