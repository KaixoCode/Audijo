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
		- ChannelInfo_
		- StreamParameters_
		- :ref:`Device Information`
			- DeviceInfo_
			- DeviceInfoAsio_
			- DeviceInfoWasapi_
	- :ref:`Enums`
		- SettingValues_
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

.. _ChannelInfo:
.. doxygenstruct:: Audijo::ChannelInfo
    :members:

.. _StreamParameters:
.. doxygenstruct:: Audijo::StreamParameters
    :members:

Device Information
~~~~~~~~~~~~~~~~~~

.. _DeviceInfo:
.. doxygenstruct:: Audijo::DeviceInfo
    :members:

.. _DeviceInfoAsio:
.. doxygenstruct:: Audijo::DeviceInfo< Asio >
    :members:

.. _DeviceInfoWasapi:
.. doxygenstruct:: Audijo::DeviceInfo< Wasapi >
    :members:

Enums
-----

.. _SettingValues:
.. doxygenenum:: Audijo::SettingValues
    :outline:
	
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
        // Make a Wasapi Stream object
        Stream<Wasapi> _stream;
        
        // Set the callback, this can be a (capturing) lambda, but also a function pointer
        _stream.SetCallback([&](double** input, double** output, CallbackInfo info)
            {   // generate a simple sinewave
                static int _counter = 0;
                for (int i = 0; i < info.bufferSize; i++, _counter++)
                    for (int j = 0; j < info.outputChannels; j++)
                        output[j][i] = std::sin(_counter * 0.01) * 0.5;
            });
        
        // Open the stream with default settings, and then start the stream
        _stream.OpenStream({ 
            .input = NoDevice, 
            .output = Default, 
            .bufferSize = Default, 
            .sampleRate = Default });
        _stream.StartStream();
        
        // Infinite loop to halt the program.
        while (true);
    }
