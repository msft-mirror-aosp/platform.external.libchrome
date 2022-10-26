#!/bin/bash
# Add libchrome namespace to libchrome's perfetto stubs.
#
# chromeos-base/perfetto_simple_producer and possibly other libchrome clients
# include the real perfetto as their dependency. On the other hand, they also uses
# parts of libchrome which will be indirectly including the trace_event_stub.h and
# causes redefinition error.
#
# Change to perfetto_libchrome namspace for the stub ones used within libchrome
# only.

for f in $(grep -rl perfetto:: --exclude-dir=libchrome_tools/); do
  sed -i 's/perfetto::/perfetto_libchrome::/g' ${f}
done

for f in $(grep -rl protozero:: --exclude-dir=libchrome_tools/); do
  sed -i 's/protozero::/protozero_libchrome::/g' ${f}
done

for f in $(grep -rl "namespace perfetto {" --exclude-dir=libchrome_tools/); do
  sed -i 's/namespace perfetto {/namespace perfetto_libchrome {/g' ${f}
done

for f in $(grep -rl "namespace protozero {" --exclude-dir=libchrome_tools/); do
  sed -i 's/namespace protozero {/namespace protozero_libchrome {/g' ${f}
done
