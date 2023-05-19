.. _ldsw_sample:

LDSW
####

.. contents::
   :local:
   :depth: 2

The LDSW (Load Switches) sample demonstrates how to control PMIC load switches with the use of npmx drivers.

Requirements
************

The sample supports the following development kits:

.. table-from-sample-yaml::

The sample also requires an nPM1300 EK.

Overview
********

This sample uses the LDSW driver to control the enable mode for a load switch either through the software or GPIOs.

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

Configuration options
=====================

Check and configure the following sample-specific Kconfig options to specify which testcase you want to run:

.. _CONFIG_TESTCASE_SW_ENABLE:

CONFIG_TESTCASE_SW_ENABLE
  This sample configuration enables controlling LDSWs through the software.
  In an infinite loop, switches are enabled and disabled alternately, with a delay of 5 seconds in between.

.. _CONFIG_TESTCASE_GPIO_ENABLE:

CONFIG_TESTCASE_GPIO_ENABLE
  This sample configuration enables controlling LDSWs through GPIOs.
  GPIO1 and GPIO2 are switched to the input mode, and selected GPIO pins (`NPMX_LDSW_GPIO_1` and `NPMX_LDSW_GPIO2`) are configured to control the switches.
  When you change the input states of the selected GPIOs, you should see voltage changes in both 1 and 2 load switches' outputs.

.. note::

   Only one Kconfig option may be enabled at a time.

Building and running
********************

.. |sample path| replace:: :file:`samples/ldsw`

.. include:: /includes/build_and_run.txt

Testing
=======

|test_sample|

#. |connect_kit|
#. |connect_terminal|
#. Connect a multimeter to **LSOUT1** and **LSOUT2**.
#. For the `CONFIG_TESTCASE_GPIO_ENABLE`_ testcase, do the following:

   a. Connect the **GPIO1** EK pin to GND and the **GPIO2** EK pin to VDD.
   #. Check if output voltage on the **LSOUT1** EK pin is 0.0 V and on the **LSOUT2** EK pin is 1.7 V.
   #. Connect the **GPIO1** EK pin to VDD and the **GPIO2** EK pin to GND.
   #. Check if output voltage on the **LSOUT1** EK pin is 3.3 V and on the **LSOUT2** EK pin is 0.0 V.

Dependencies
************

This sample uses drivers from npmx.
