#!/usr/bin/env python
# Copyright 2022 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections


class DependencyGraph:
    """Structure to maintain object dependencies by used/wanted symbols."""

    def __init__(self):
        """Initializes."""
        self.provides = {}
        self.wants = {}
        self.symbol_provided = collections.defaultdict(list)
        self.symbol_used = collections.defaultdict(list)

    def add_object(self,
                   object_name,
                   provides,
                   wants,
                   ignore_duplicate_symbol_error=False):
        """
        Adds an object into the graph.

        Args:
          object_name: name or path (a unique identifier) of the object.
          provides: list of symbols it provides.
          wants: list of symbols it wants.
          ignore_duplicate_symbol_error: do not assert if two objects provides
            the same symbol.
        """
        assert object_name not in self.provides, object_name
        assert object_name not in self.wants, object_name
        self.provides[object_name] = provides
        self.wants[object_name] = wants

        for symbol in provides:
            if not ignore_duplicate_symbol_error:
                assert symbol not in self.symbol_provided, 'Object %s provides %s but already provided by %s' % (
                    object_name, symbol, self.symbol_provided[symbol])
                self.symbol_provided[symbol].append(object_name)

        for symbol in wants:
            self.symbol_used[symbol].append(object_name)

    def get_reverse_dependencies(self, object_name):
        """
        Gets list of objects that depends on object_name.

        Args:
          object_name: name or path (a unique identifier) of the object.
        """
        symbols_provided = self.provides[object_name]
        ret = set()
        for symbol in symbols_provided:
            ret |= set(self.symbol_used[symbol])

        return list(ret)

    def get_dependencies(self, object_name):
        """
        Gets list of objects that object_name depends on.

        Args:
          object_name: name or path (a unique identifier) of the object.
        """
        symbols_used = self.wants[object_name]
        ret = set()
        for symbol in symbols_used:
            ret |= set(self.symbol_provided[symbol])

        return list(ret)

    def get_all_dependencies(self, object_name, reverse=False):
        """
        Gets list of objects that object_name depends on, recursively.

        Args:
          object_name: name or path (a unique identifier) of the object.
          reverse: set to True to get reverse dependency recursively.
        """
        ret = set()
        pending_queue = collections.deque()
        processed = set()

        ret.add(object_name)
        pending_queue.append(object_name)

        while pending_queue:
            item = pending_queue.popleft()
            processed.add(item)
            if reverse:
                dependencies = self.get_reverse_dependencies(item)
            else:
                dependencies = self.get_dependencies(item)
            ret |= set(dependencies)
            for dependency in dependencies:
                if dependency in processed:
                    continue
                if dependency in pending_queue:
                    continue
                pending_queue.append(dependency)

        return list(ret)
