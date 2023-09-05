# pycbuf

`pycbuf` is a Python wrapper around cbuf. It provides a comprehensive set of python interface to cbuf files, allowing to browser and decode any and all types on a `cbuf`.

Created by Lawrence Ibarria at Verdant Robotics, [lawrence@verdantrobotics.com](mailto:lawrence@verdantrobotics.com).

The wrapper provides a single class, `CBufReader`, which can read ulogs.

## Reading ulog using CBufReader

CBufReader can be created from a ulog path or used in a `with` statement like so:

```python
from pycbuf.pycbuf import CBufReader
with CBufReader(ulog_path) as reader:
    if reader.open():
        for msg in reader:
            print(msg)
        reader.close()
```

After it has been opened, it can be used an iterator over messages, as shown above as well.

Available methods:

- `create(...)`: Create a CBufReader to access ulogs. All arguments are optional:

        - ulog_path (str): the path of the .ulog folder to process
        - role_filter (str or list): roles to filter, e.g. ['spraybox0']. Defaults to all.
        - message_filter (str or list): messages to filter (frameinfo). Defaults to all
        - early_time (float): Time in seconds where to start processing messages
        - late_time (float): Time in seconds when to stop processing messages

- `set_roles(...)`: roles to filter, e.g. ['spraybox0'], or clear filter if no argument.

- `set_messages(...)`: messages to filter (frameinfo), or clear filter if no argument.

- `set_time(...)`: set (early_time, late_time) using a list or tuple, or clear time filter if no argument.

- `open(self)`: Using the previously set ulogpath and filters, open the log for processing

- `get_message(...)`: get next message, same as next(reader)

- `close(self)`: Free all memory and related informaton on the ulog.

Finally, there is also a printable representation that provides status along with active filters:

```python
> print(f"Status: \n {repr(reader)}\n")
Status:
 Closed | Role Filters: spraybox0 | Message Filters: frameinfo | Time filters: None
```

## Streaming CBuf using CBufReader

CBufReader can do streaming upon binary arrays. To do so:

```python
from pycbuf.pycbuf import CBufReader
with CBufReader() as reader:
    cbuf_name = "messages::message_name_1"
    cbuf_schema = """namespace messages {
        struct message_name_1 {
            // fields are omitted
        }
    }"""
    binary_array = b'' # the binary array is a serialized cbuf
    reader.set_cbuf_schema(cbuf_name, cbuf_schema)
    msg = reader.get_cbuf_from_array(binary_array)
```

Available methods:
- `set_cbuf_schema(...)`: Specify the schema of a CBuf. All arguments are required:
        - cbuf_name (str): the name of the cbuf, with namespace
        - cbuf_schema (str): the schema of the cbuf

- `get_cbuf_from_array(...)`: Convert a binary array to a CBuf. All arguments are required:
        - cbuf_name (str): the name of the cbuf, with namespace
        - binary_array (bytes): the serialized cbuf binary array
