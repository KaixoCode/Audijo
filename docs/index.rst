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
	- :ref:`Buffer`
		- Buffer_
		- Parallel_
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
	- :ref:`Play noise`

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

Buffer
------

.. _Buffer:
.. doxygenclass:: Audijo::Buffer
	:members:
	
.. _Parallel:
.. doxygenclass:: Audijo::Parallel
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

Play noise
----------

.. code-block:: cpp
	
    #include "Audijo.hpp"
    
    using namespace Audijo;
    
    int main()
    {
        // Make a Wasapi Stream object
        Stream<Wasapi> _stream;
        
        // Set the callback, this can be a (capturing) lambda, but also a function pointer
        _stream.Callback([&](Buffer<float>& input, Buffer<float>& output, CallbackInfo info) {   
            for (auto& _frame : output) // Loop through all frames in the output
                for (auto& _channel : _frame) // Loop through all the channels in the frame
                    _channel = 0.5 * ((std::rand() % 10000) / 10000. - 0.5); // Generate noise
        });
        
        // Open the stream with default settings, and then start the stream
        _stream.Open({ 
            .input = NoDevice, 
            .output = Default, 
            .bufferSize = Default, 
            .sampleRate = Default 
        });
        
        // Start the stream
        _stream.Start();
        
        // Wait until we get an input from the console.
        std::cin.get();
        
        // Close the stream
        _stream.Close();
    }
