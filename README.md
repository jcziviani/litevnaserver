# LiteVNAServer

LiteVNAServer is an HTTP server for querying data (in JSON format) from a
LiteVNA 64 device. It uses device calibration data.

## Usage

```
litevnaserver [options]

    Options:
        --version                    Show version information.
        --help                       Display this information.
        -com-port=<name>             (required) serial port where LiteVNA device is connected.
        -tcp-port=<number>           (required) tcp port where LiteVNAServer will listen for requests.
        -logger-categories=<options> Comma separated options: http_server,lite_vna,info,error,all (default info,error).
        -logger-file=<file-name>     Logger output file (do not write to file by default).
```

### Example:

`litevnaserver -com-port=COM4 -tcp-port=8888 -logger-categories=lite_vna,info,error`

## Requests

Clients must send an HTML GET request with url containing all the following
parameters:

- **start:** sweep start frequency in Hz.
- **step:** sweep step frequency in Hz.
- **points:** number of sweep frequency points.

Example: http://localhost:8888/litevna?start=4300000000&step=10000000&points=2

## Return value

For a successful call, returns a JSON with a `result` field containing the
scanned data.

Example:

```json
{
    "result": [
        {
            "freq": 4300000000,
            "s11": {
                "log_mag": -11.0299,
                "phase": -159.707,
                "swr": 1.78113
            },
            "s21": {
                "log_mag": -73.0412,
                "phase": -121.084
            }
        },
        {
            "freq": 4310000000,
            "s11": {
                "log_mag": -11.2397,
                "phase": -161.013,
                "swr": 1.75546
            },
            "s21": {
                "log_mag": -67.4901,
                "phase": -88.1427
            }
        }
    ]
}
```

If an error occurs, returns a JSON with an `error` field with a description.

Example:

```json
{
    "error": "missing 'start' parameter"
}
```

## Installation

Just run the `litevnaserver` executable using command prompt.

### Windows

Build the executable using Visual Studio 2022 Community Edition.

### Linux

Run `make` command to build the executable. Only a compiler (g++ or clang++) is
required, there are no external dependencies.

```bash
make
```
