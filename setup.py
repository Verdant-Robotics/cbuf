from pathlib import Path
from skbuild import setup

VERSION = "0.1.1"

this_directory = Path(__file__).parent
long_description = (this_directory / "README.md").read_text()

setup(
    # Package metadata
    name="pycbuf",
    version=VERSION,
    description="Python implementation of CBUF message serialization",
    long_description=long_description,
    long_description_content_type="text/markdown",
    license="Apache 2.0",
    author="Verdant Robotics",
    author_email="info@verdantrobotics.com",
    url="https://github.com/Verdant-Robotics/cbuf",
    packages=["pycbuf"],
    python_requires=">=3.10",
    # skbuild options
    cmake_args=[
        "-DCMAKE_BUILD_TYPE=RelWithDebInfo",
        "-DENABLE_HJSON=OFF",
        "-DENABLE_CBUF_TESTS=OFF",
    ],
)
