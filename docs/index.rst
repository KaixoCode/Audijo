Welcome to Audijo's documentation!
===================================

Audijo repository: https://github.com/KaixoCode/Audijo


.. toctree::
   :maxdepth: 10
   :caption: Contents:



Content
=======

- Documentation_
	- :ref:`Stream`
		- Stream_
		- StreamAsio_
		- StreamWasapi_
	- :ref:`Structs`
		- CallbackInfo_
		- DeviceInfo_
		- Parameters_
		- StreamSettings_
	- :ref:`Enums`
		- SampleFormat_
		- Api_
		- Error_
- Examples_
	- :ref:`Simple`

Documentation
==============

Stream
------

.. _Stream:
.. doxygenclass:: Audijo::Stream
	:members:
	
.. _StreamAsio:
.. doxygenclass:: Audijo::Stream< Asio >
	:members:

.. _StreamWasapi:
.. doxygenclass:: Audijo::Stream< Wasapi >
	:members:

Structs
-------

.. _CallbackInfo:
.. doxygenstruct:: Audijo::CallbackInfo
    :members:
	
.. _DeviceInfo:
.. doxygenstruct:: Audijo::DeviceInfo
    :members:

.. _Parameters:
.. doxygenstruct:: Audijo::Parameters
    :members:

.. _StreamSettings:
.. doxygenstruct:: Audijo::StreamSettings
    :members:

Enums
-----

.. _SampleFormat:
.. doxygenenum:: Audijo::SampleFormat
    :outline:
	
.. _Api:
.. doxygenenum:: Audijo::Api
    :outline:

.. _Error:
.. doxygenenum:: Audijo::Error
    :outline:

Examples
========

Simple
------

.. code-block:: cpp
	
    #include "Audijo.hpp"
    
    using namespace Audijo;
    
    int main()
    {
        // Make an Asio Stream object
        Stream<Asio> _stream;
        
        // Set the callback, this can be a (capturing) lambda, but also a function pointer
        _stream.SetCallback([&](double** input, double** output, CallbackInfo info)
            {   // generate a simple sinewave
                static int _counter = 0;
                for (int i = 0; i < info.bufferSize; i++, _counter++)
                    for (int j = 0; j < info.outputChannels; j++)
                        output[j][i] = std::sin(_counter * 0.01) * 0.5;
            });
        
        // Stream settings, like sample rate/buffer size/device id
        StreamSettings _settings;
        _settings.bufferSize = 256;
        _settings.sampleRate = 48000;
        _settings.output.deviceId = 0;
		
        // Open the stream with the settings, and then start the stream
        _stream.OpenStream(_settings);
        _stream.StartStream();
        
        // Infinite loop to halt the program.
        while (true);
    }
