#####################################
Documentation Contribution Guidelines
#####################################

Introduction
------------

This is a guideline for the Arduino ESP32 project documentation. The idea for this guideline is to show how to start collaborating on the project.

The guideline works to give you the directions and to keep the documentation more concise, helping users to better understand the structure.

About Documentation
-------------------

We all know how important documentation is. This project is no different.

This documentation was created in a collaborative and open way, letting everyone contribute, from a small typo fix to a new chapter writing. We try to motivate our community by giving all the support needed through this guide.

The documentation is in **English only**. Future translations can be added since we finish the essential content in English first.

How to Collaborate
------------------

Everyone with some knowledge to share is welcome to collaborate.

One thing you need to consider is the fact that your contribution must be concise and assertive since it will be used by people developing projects. The information is very important for everyone, be sure you are not making the developer's life harder!

Documentation Guide
-------------------

This documentation is based on the `Sphinx`_ with `reStructuredText`_ and hosted by `ReadTheDocs`_.

If you want to get started with `Sphinx`_, see the official documentation:

* `Documentation Index <https://www.sphinx-doc.org/en/master/usage/restructuredtext/index.html>`_
* `Basics <https://www.sphinx-doc.org/en/master/usage/restructuredtext/basics.html>`_
* `Directives <https://www.sphinx-doc.org/en/master/usage/restructuredtext/directives.html>`_

First Steps
***********

Before starting your collaboration, you need to get the documentation source code from the Arduino-ESP32 project.

* **Step 1** - Fork the `Arduino-ESP32`_ to your GitHub account.
* **Step 2** - Check out the recently created fork.
* **Step 3** - Create a new branch for the changes/addition to the docs.
* **Step 4** - Write!

Requirements
************

To properly work with the documentation, you need to install some packages in your system.

.. code-block::

    pip install -U Sphinx
    pip install -r requirements.txt

The requirements file is under the ``docs`` folder.

Using Visual Studio Code
************************

If you are using the Visual Studio Code, you can install some extensions to help you while writing documentation.

`reStructuredText Pack <https://marketplace.visualstudio.com/items?itemName=lextudio.restructuredtext-pack>`_

We also recommend you install to grammar check extension to help you to review English grammar.

`Grammarly <https://marketplace.visualstudio.com/items?itemName=znck.grammarly>`_

Building
********

To build the documentation and generate the HTLM files, you can use the following command inside the ``docs`` folder. After a successful build, you can check the files inside the `build/html` folder.

.. code-block::

    make html

This step is essential to ensure that there are no syntax errors and also to see the final result.

If everything is ok, you will see some output logs similar to this one:

.. code-block::

    Running Sphinx v2.3.1
    loading pickled environment... done
    building [mo]: targets for 0 po files that are out of date
    building [html]: targets for 35 source files that are out of date
    updating environment: [extensions changed ('sphinx_tabs.tabs')] 41 added, 3 changed, 0 removed
    reading sources... [100%] tutorials/tutorials                                                                                                                                                                                                                                                             
    looking for now-outdated files... none found
    pickling environment... done
    checking consistency... done
    preparing documents... done
    writing output... [100%] tutorials/tutorials                                                                                                                                                                                                                                                              
    generating indices...  genindexdone
    writing additional pages...  searchdone
    copying images... [100%] tutorials/../_static/tutorials/peripherals/tutorial_peripheral_diagram.png                                                                                                                                                                                                       
    copying static files... ... done
    copying extra files... done
    dumping search index in English (code: en)... done
    dumping object inventory... done
    build succeeded.

The HTML pages are in build/html.

Sections
--------

The Arduino ESP32 is structured in some sections to make it easier to maintain. Here is a brief description of this structure.

API
***

In this section, you will include all the documentation about drivers, libraries and any other related to the core.

Boards
******

Here is the place to add any special guide on the development boards, pin layout, schematics, and any other relevant content.

Common
******

In this folder, you can add all common information used in several different places. This helps to maintain the documentation easily maintainable.


Guides
******

This is the place to add the guides for common applications, IDEs configuration, and any other information that can be used as a guideline.

Tutorials
*********

If you want to add a specific tutorial related to the Arduino for ESP32, this is the place. The intention is not to create a blog or a demo area, but this can be used to add some complex description or to add some more information about APIs.

Images and Assets
*****************

All the files used on the documentation must be stored in the ``_static`` folder. Be sure that the content used is not with any copyright restriction.

Documentation Rules
-------------------

Here are some guidelines to help you. We also recommend copying a sample file from the same category you are creating.

This will help you to follow the structure as well as to get inspired.

Basic Structure
***************

To help you create a new section from scratch, we recommend you include this structure in your content if it applies.

* Brief description of the document.
* Description of the peripheral, driver, protocol, including all different modes and configurations.
* Description of each public function, macros, and structs.
* If it's relevant for the user or if it's used in the example must be included in the description.
* How to use.
* Example code from the examples folder or code snippet.

Heading Levels
**************

The heading levels used on this documentation are:

* **H1**: - (Dash)
* **H2**: * (Asterisk)
* **H3**: ^ (Circumflex)
* **H4**: # (Sharp)

Code Block
**********

To add a code block, you can use the following structure:

.. code-block::

    .. code-block:: arduino
        bool begin(); //Code example

Links
*****

To include links to external content, you can use two ways. The first 

.. code-block::

    `Arduino Wire Library`_

    _Arduino Wire Library: https://www.arduino.cc/en/reference/wire

or

.. code-block::

    `Arduino Wire Library <https://www.arduino.cc/en/reference/wire>`_

Images
******

To include images in the docs, first, add all the files into the ``_static`` folder with a filename that makes sense for the topic.

After that, you can use the following structure to include the image in the docs.

.. code-block::

    .. figure:: ../_static/arduino_i2c_master.png
        :align: center
        :width: 720
        :figclass: align-center

You can adjust the ``width`` according to the image size.

Be sure the file size does not exceed 600kB.

Support
*******

If you need support on the documentation, you can ask a question as a discussion `here <https://github.com/espressif/arduino-esp32/discussions>`_.

.. _Arduino-ESP32: https://github.com/espressif/arduino-esp32
.. _Sphinx: https://www.sphinx-doc.org/en/master/
.. _ReadTheDocs: https://readthedocs.org/
.. _reStructuredText: https://docutils.sourceforge.io/rst.html
