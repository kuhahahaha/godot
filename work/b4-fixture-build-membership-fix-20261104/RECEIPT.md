# Fixture build-membership correction

The first actual engine build at40a7 failed because tests/SCsub recursively compiles every *.cpp, including the standalone guard harness template whose production functions are inserted only by run.py. The raw failed build and its exact source/result identities are retained separately.

The template is now harness.cpp.in; its bytes are unchanged from40a7. The runner reads that exact new path. The actual SCsub glob excludes both the removed .cpp and the .cpp.in template, and the fixture again passes five SPIR-V variants, forty CPU control/interval cases and three targeted red mutations. No Metal production source or shader was changed. The next full engine build remains a separate acceptance step and must use the new canonical commit.
