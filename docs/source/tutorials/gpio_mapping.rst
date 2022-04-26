############
GPIO Mapping
############

Introduction
------------

This tutorial will guide you to select the right **GPIO** to be used. Some GPIOs have special features and some have restrictions.

Choosing the right **GPIO** can save time and avoid any non-expected behavior.

For all ESP32 SoC families, the GPIOs can have distinct functions or not be available depending on the module type. Be sure to check this before changing your project module to another one.

ESP32 Modules
-------------

Here is the GPIO mapping for the ESP32 modules.

.. tabs::

   .. tab:: ESP32-WROOM-32E/U

      =====================  ===========================================
      GPIO                   Description and Usage Restriction
      =====================  ===========================================
      Restricted GPIOs       6
      Straping Pins          5
      Available GPIOs        17
      Input Only             4
      **Total GPIOs**        **32**
      =====================  ===========================================

      =====================  ===========================================
      GPIO                   Description and Usage Restriction
      =====================  ===========================================
      GPIO00                 I/O & Boot Mode
      GPIO01                 I/O & Download UART
      GPIO02                 Strapping: Input/Output
      GPIO03                 I/O & Download UART
      GPIO04                 Input/Output
      GPIO05                 Strapping: Input/Output
      **GPIO06**             **Restricted for SPI Flash**
      **GPIO07**             **Restricted for SPI Flash**
      **GPIO08**             **Restricted for SPI Flash**
      **GPIO09**             **Restricted for SPI Flash**
      **GPIO10**             **Restricted for SPI Flash**
      **GPIO11**             **Restricted for SPI Flash**
      GPIO12                 Strapping: Input/Output
      GPIO13                 Input/Output
      GPIO14                 Input/Output
      GPIO15                 Strapping: Input/Output
      GPIO16                 Input/Output
      GPIO17                 Input/Output
      GPIO18                 Input/Output
      GPIO19                 Input/Output
      *GPIO20*               *Not Available*
      GPIO21                 Input/Output
      GPIO22                 Input/Output
      GPIO23                 Input/Output
      *GPIO24*               *Not Available*
      GPIO25                 Input/Output
      GPIO26                 Input/Output
      GPIO27                 Input/Output
      *GPIO28*               *Not Available*
      *GPIO29*               *Not Available*
      *GPIO30*               *Not Available*
      *GPIO31*               *Not Available*
      GPIO32                 Input/Output
      GPIO33                 Input/Output
      GPIO34                 Input Only
      GPIO35                 Input Only
      GPIO36                 Input Only
      *GPIO37*               *Not Available*
      *GPIO38*               *Not Available*
      GPIO39                 Input Only
      =====================  ===========================================

   .. tab:: ESP32-WROVER-E/IE

      =====================  ===========================================
      GPIO                   Description and Usage Restriction
      =====================  ===========================================
      GPIO00                 I/O & Boot Mode
      GPIO01                 I/O & Download UART
      GPIO02                 Strapping: Input/Output
      GPIO03                 I/O & Download UART
      GPIO04                 Input/Output
      GPIO05                 Strapping: Input/Output
      **GPIO06**             **Restricted for SPI Flash**
      **GPIO07**             **Restricted for SPI Flash**
      **GPIO08**             **Restricted for SPI Flash**
      **GPIO09**             **Restricted for SPI Flash**
      **GPIO10**             **Restricted for SPI Flash**
      **GPIO11**             **Restricted for SPI Flash**
      GPIO12                 Strapping: Input/Output
      GPIO13                 Input/Output
      GPIO14                 Input/Output
      GPIO15                 Strapping: Input/Output
      *GPIO16*               *Not Available*
      *GPIO17*               *Not Available*
      GPIO18                 Input/Output
      GPIO19                 Input/Output
      *GPIO20*               *Not Available*
      GPIO21                 Input/Output
      GPIO22                 Input/Output
      GPIO23                 Input/Output
      *GPIO24*               *Not Available*
      GPIO25                 Input/Output
      GPIO26                 Input/Output
      GPIO27                 Input/Output
      *GPIO28*               *Not Available*
      *GPIO29*               *Not Available*
      *GPIO30*               *Not Available*
      *GPIO31*               *Not Available*
      GPIO32                 Input/Output
      GPIO33                 Input/Output
      GPIO34                 Input Only
      GPIO35                 Input Only
      GPIO36                 Input Only
      GPIO37                 Input Only
      GPIO38                 Input Only
      GPIO39                 Input Only
      =====================  ===========================================

   .. tab:: ESP32-MINI-1/U

      =====================  ===========================================
      GPIO                   Description and Usage Restriction
      =====================  ===========================================
      GPIO00                 I/O & Boot Mode
      GPIO01                 I/O & Download UART
      GPIO02                 Strapping: Input/Output
      GPIO03                 I/O & Download UART
      GPIO04                 Input/Output
      GPIO05                 Strapping: Input/Output
      **GPIO06**             **Restricted for SPI Flash**
      **GPIO07**             **Restricted for SPI Flash**
      **GPIO08**             **Restricted for SPI Flash**
      GPIO09                 Input/Output
      GPIO10                 Input/Output
      **GPIO11**             **Restricted for SPI Flash**
      GPIO12                 Strapping: Input/Output
      GPIO13                 Input/Output
      GPIO14                 Input/Output
      GPIO15                 Strapping: Input/Output
      *GPIO16*               *Not Available*
      *GPIO17*               *Not Available*
      GPIO18                 Input/Output
      GPIO19                 Input/Output
      *GPIO20*               *Not Available*
      GPIO21                 Input/Output
      GPIO22                 Input/Output
      GPIO23                 Input/Output
      *GPIO24*               *Not Available*
      GPIO25                 Input/Output
      GPIO26                 Input/Output
      GPIO27                 Input/Output
      *GPIO28*               *Not Available*
      *GPIO29*               *Not Available*
      *GPIO30*               *Not Available*
      *GPIO31*               *Not Available*
      GPIO32                 Input/Output
      GPIO33                 Input/Output
      GPIO34                 Input Only
      GPIO35                 Input Only
      GPIO36                 Input Only
      *GPIO37*               *Not Available*
      *GPIO38*               *Not Available*
      GPIO39                 Input Only
      =====================  ===========================================

   .. tab:: ESP32-WROOM-DA

      =====================  ===========================================
      GPIO                   Description and Usage Restriction
      =====================  ===========================================
      GPIO00                 I/O & Boot Mode
      GPIO01                 I/O & Download UART
      **GPIO02**             **Antenna Switch**
      GPIO03                 I/O & Download UART
      GPIO04                 Input/Output
      GPIO05                 Strapping: Input/Output
      **GPIO06**             **Restricted for SPI Flash**
      **GPIO07**             **Restricted for SPI Flash**
      **GPIO08**             **Restricted for SPI Flash**
      **GPIO09**             **Restricted for SPI Flash**
      **GPIO10**             **Restricted for SPI Flash**
      **GPIO11**             **Restricted for SPI Flash**
      GPIO12                 Strapping: Input/Output
      GPIO13                 Input/Output
      GPIO14                 Input/Output
      GPIO15                 Strapping: Input/Output
      GPIO16                 Input/Output
      GPIO17                 Input/Output
      GPIO18                 Input/Output
      GPIO19                 Input/Output
      *GPIO20*               *Not Available*
      GPIO21                 Input/Output
      GPIO22                 Input/Output
      GPIO23                 Input/Output
      *GPIO24*               *Not Available*
      **GPIO25**             **Antenna Switch**
      GPIO26                 Input/Output
      GPIO27                 Input/Output
      *GPIO28*               *Not Available*
      *GPIO29*               *Not Available*
      *GPIO30*               *Not Available*
      *GPIO31*               *Not Available*
      GPIO32                 Input/Output
      GPIO33                 Input/Output
      GPIO34                 Input Only
      GPIO35                 Input Only
      GPIO36                 Input Only
      *GPIO37*               *Not Available*
      *GPIO38*               *Not Available*
      GPIO39                 Input Only
      =====================  ===========================================

ESP32-S2 Modules
----------------

ESP32-C3 Modules
----------------

ESP32-S3 Modules
----------------
