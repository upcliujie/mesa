Perfetto producers
==================

Mesa contains a `Perfetto producer <https://perfetto.dev/docs/concepts/service-model>`__ with data-sources for some of its drivers. A producer is a client process for the Perfetto tracing service, while a data-source is a capability, exposed by a producer, which provides tracing data. The following data-sources are currently available:

- Panfrost performance counters
- Intel performance counters
- Freedreno performance counters (WIP)

Build
-----

This section guides you through the building process assuming you are compiling on Ubuntu.

1. Install dependencies:

   .. code-block:: console

      apt install build-essential meson cmake libwayland-dev rapidjson-dev libdrm-dev libprotobuf-dev libgtest-dev libdocopt-dev

2. Generate the project:

   .. code-block:: console

      meson build -Dperfetto=true -Dgallium-drivers=swrast,panfrost,iris

3. Compile and run tests (it may take a while):

   .. code-block:: console

      ninja -C build test

Run
---

To capture a trace with perfetto you need to take the following steps:

1. Create a `trace config <https://perfetto.dev/#/trace-config.md>`__, which is a json formatted text file with extension ``.cfg``, or use one of those under the ``src/tool/pps/cfg`` directory.

2. Copy the config file to ``perfetto/test/configs``. Under this directory you can also find more example of trace configs.

3. Change directory to ``perfetto`` and run a `convenience script <https://perfetto.dev/#/running.md>`__ to start the tracing service:

   .. code-block:: console

      cd <path>/perfetto
      CONFIG=test.cfg OUT=out/build ./tools/tmux

4. Start other producers you may need, like ``pps-producer``.

5. Start perfetto under the tmux session initiated in step 3.

6. Once tracing has finished, you can detach from tmux with :kbd:`Ctrl+b`, :kbd:`d`, and the convenience script should automatically copy the trace files into ``$HOME/Downloads``.

7. Go to `ui.perfetto.dev <https://ui.perfetto.dev>`__ and upload ``$HOME/Downloads/trace.protobuf`` by clicking on **Open trace file**.

GPU producer
~~~~~~~~~~~~

The GPU producer contains at the current state a data-source able to query performance counters using a Panfrost driver for Arm Mali devices, or an Intel driver for Intel Graphics devices.

Panfrost driver
^^^^^^^^^^^^^^^

The Panfrost driver uses unstable ioctls that behave correctly on kernel version `5.4.23+ <https://lwn.net/Articles/813601/>`__ and `5.5.7+ <https://lwn.net/Articles/813600/>`__.

To run the producer, follow these two simple steps:

1. Enable Panfrost unstable ioctls via kernel parameter:

   .. code-block:: console

      modprobe panfrost unstable_ioctls=1

2. Run the producer:

   .. code-block:: console

      ./build/pps-producer

Intel driver
^^^^^^^^^^^^

The Intel driver needs root access to read system-wide `RenderBasic <https://software.intel.com/content/www/us/en/develop/documentation/vtune-help/top/reference/gpu-metrics-reference.html>`__ performance counters, so you can simply run it with sudo:

.. code-block:: console

   sudo ./build/pps-producer

Troubleshooting
---------------

Tmux
~~~~

If the convenience script ``tools/tmux`` keeps copying artifacts to your ``SSH_TARGET`` without starting the tmux session, make sure you have ``tmux`` installed in your system.

.. code-block:: console

   apt install tmux

Missing counter names
~~~~~~~~~~~~~~~~~~~~~

If the trace viewer shows a list of counters with a description like ``gpu_counter(#)`` instead of their proper names, maybe you had a data loss due to the trace buffer being full and wrapped.

In order to prevent this loss of data you can tweak the trace config file in two different ways:

- Increase the size of the buffer in use:

  .. code-block:: javascript

      buffers {
          size_kb: 2048,
          fill_policy: RING_BUFFER,
      }

- Periodically flush the trace buffer into the output file:

  .. code-block:: javascript

     write_into_file: true
     file_write_period_ms: 250
