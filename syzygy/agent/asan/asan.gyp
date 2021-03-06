# Copyright 2012 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'syzyasan_rtl_lib',
      'type': 'static_library',
      'includes': ['../../build/masm.gypi'],
      'sources': [
        'allocators.h',
        'allocators_impl.h',
        'block.cc',
        'block.h',
        'block_impl.h',
        'block_utils.cc',
        'block_utils.h',
        'circular_queue.h',
        'circular_queue_impl.h',
        'constants.cc',
        'constants.h',
        'crt_interceptors.cc',
        'crt_interceptors.h',
        'error_info.cc',
        'error_info.h',
        'heap.cc',
        'heap.h',
        'heap_checker.cc',
        'heap_checker.h',
        'heap_manager.h',
        'heap_managers/block_heap_manager.cc',
        'heap_managers/block_heap_manager.h',
        'heaps/ctmalloc_heap.cc',
        'heaps/ctmalloc_heap.h',
        'heaps/internal_heap.cc',
        'heaps/internal_heap.h',
        'heaps/large_block_heap.cc',
        'heaps/large_block_heap.h',
        'heaps/simple_block_heap.cc',
        'heaps/simple_block_heap.h',
        'heaps/win_heap.cc',
        'heaps/win_heap.h',
        'heaps/zebra_block_heap.cc',
        'heaps/zebra_block_heap.h',
        'iat_patcher.cc',
        'iat_patcher.h',
        'logger.cc',
        'logger.h',
        'memory_interceptors.cc',
        'memory_interceptors.h',
        'memory_notifier.h',
        'memory_notifiers/null_memory_notifier.h',
        'memory_notifiers/shadow_memory_notifier.cc',
        'memory_notifiers/shadow_memory_notifier.h',
        'memory_redirectors.asm',
        'page_allocator.h',
        'page_allocator_impl.h',
        'page_protection_helpers.cc',
        'page_protection_helpers.h',
        'quarantine.h',
        'quarantines/sharded_quarantine.h',
        'quarantines/sharded_quarantine_impl.h',
        'quarantines/size_limited_quarantine.h',
        'quarantines/size_limited_quarantine_impl.h',
        'rtl_impl.cc',
        'rtl_impl.h',
        'rtl_utils.cc',
        'rtl_utils.h',
        'registry_cache.cc',
        'registry_cache.h',
        'runtime.cc',
        'runtime.h',
        'runtime_util.cc',
        'runtime_util.h',
        'shadow.cc',
        'shadow.h',
        'shadow_impl.h',
        'shadow_marker.cc',
        'shadow_marker.h',
        'stack_capture_cache.cc',
        'stack_capture_cache.h',
        'system_interceptors.cc',
        'system_interceptors.h',
        'timed_try.h',
        'timed_try_impl.h',
        'windows_heap_adapter.cc',
        'windows_heap_adapter.h',
      ],
      'dependencies': [
        '<(src)/syzygy/crashdata/crashdata.gyp:crashdata_lib',
        '<(src)/syzygy/common/common.gyp:common_lib',
        '<(src)/syzygy/kasko/kasko.gyp:kasko',
        '<(src)/syzygy/trace/client/client.gyp:rpc_client_lib',
        '<(src)/syzygy/trace/common/common.gyp:trace_common_lib',
        '<(src)/syzygy/trace/rpc/rpc.gyp:logger_rpc_lib',
        '<(src)/syzygy/trace/protocol/protocol.gyp:protocol_lib',
        '<(src)/third_party/ctmalloc/ctmalloc.gyp:ctmalloc_lib',
      ],
      'export_dependent_settings': [
        # We depend on crashdata_lib, which means we can see the include
        # directories it exports via 'all_dependent_settings' or
        # 'direct_dependent_settings'. However, our dependents will have
        # this same dependency, so we forward these settings to them.
        '<(src)/syzygy/crashdata/crashdata.gyp:crashdata_lib',
      ],
    },
    {
      'target_name': 'syzyasan_rtl',
      'type': 'loadable_module',
      'includes': [
        '../agent.gypi',
        'agent_link_settings.gypi',
      ],
      'sources': [
        'gen/system_interceptors.def',
        'syzyasan_rtl.cc',
        'syzyasan_rtl.rc',
      ],
      'dependencies': [
        'syzyasan_rtl_lib',
        '<(src)/syzygy/agent/common/common.gyp:agent_common_lib',
        '<(src)/syzygy/common/common.gyp:common_lib',
        '<(src)/syzygy/core/core.gyp:core_lib',
        '<(src)/syzygy/version/version.gyp:syzygy_version',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          # This module should delay load nothing.
          'DelayLoadDLLs=': [
          ],
        },
      },
      'conditions': [
        ['pgo_phase==1', {
          'msvs_settings': {
            'VCLinkerTool': {
              # 2 corresponds to LTCG:PGINSTRUMENT.
              'LinkTimeCodeGeneration': '2',
            },
          },
        }],
        ['pgo_phase==2', {
          'msvs_settings': {
            'VCLinkerTool': {
              # 3 corresponds to LTCG:PGOPTIMIZE.
              'LinkTimeCodeGeneration': '3',
            },
          },
        }],
      ],
    },
    {
      'target_name': 'syzyasan_rtl_unittest_utils',
      'type': 'static_library',
      'sources': [
        'unittest_util.cc',
        'unittest_util.h',
      ],
      'dependencies': [
        'syzyasan_rtl_lib',
        '<(src)/base/base.gyp:base',
        '<(src)/base/base.gyp:test_support_base',
        '<(src)/syzygy/agent/common/common.gyp:agent_common_lib',
        '<(src)/syzygy/common/common.gyp:common_lib',
        '<(src)/syzygy/core/core.gyp:core_unittest_utils',
        '<(src)/syzygy/testing/testing.gyp:testing_lib',
        '<(src)/syzygy/trace/agent_logger/agent_logger.gyp:agent_logger',
        '<(src)/syzygy/trace/agent_logger/agent_logger.gyp:agent_logger_lib',
        '<(src)/testing/gmock.gyp:gmock',
        '<(src)/testing/gtest.gyp:gtest',
      ],
    },
    {
      'target_name': 'syzyasan_rtl_unittests',
      'type': 'executable',
      'sources': [
        'allocators_unittest.cc',
        'crt_interceptors_unittest.cc',
        'iat_patcher_unittest.cc',
        'logger_unittest.cc',
        'runtime_unittest.cc',
        'rtl_impl_unittest.cc',
        'rtl_unittest.cc',
        'rtl_utils_unittest.cc',
        'system_interceptors_unittest.cc',
        'block_unittest.cc',
        'block_utils_unittest.cc',
        'circular_queue_unittest.cc',
        'error_info_unittest.cc',
        'heap_checker_unittest.cc',
        'memory_interceptors_unittest.cc',
        'page_allocator_unittest.cc',
        'page_protection_helpers_unittest.cc',
        'registry_cache_unittest.cc',
        'shadow_marker_unittest.cc',
        'shadow_unittest.cc',
        'stack_capture_cache_unittest.cc',
        'timed_try_unittest.cc',
        'windows_heap_adapter_unittest.cc',
        'heaps/ctmalloc_heap_unittest.cc',
        'heaps/internal_heap_unittest.cc',
        'heaps/large_block_heap_unittest.cc',
        'heaps/simple_block_heap_unittest.cc',
        'heaps/win_heap_unittest.cc',
        'heaps/zebra_block_heap_unittest.cc',
        'heap_managers/block_heap_manager_unittest.cc',
        'memory_notifiers/shadow_memory_notifier_unittest.cc',
        'quarantines/sharded_quarantine_unittest.cc',
        'quarantines/size_limited_quarantine_unittest.cc',
        '<(src)/base/test/run_all_unittests.cc',
      ],
      'dependencies': [
        'syzyasan_rtl_lib',
        'syzyasan_rtl',
        'syzyasan_rtl_unittest_utils',
        '<(src)/base/base.gyp:base',
        '<(src)/base/base.gyp:test_support_base',
        '<(src)/syzygy/agent/common/common.gyp:agent_common_lib',
        '<(src)/syzygy/assm/assm.gyp:assm_lib',
        '<(src)/syzygy/common/common.gyp:common_lib',
        '<(src)/syzygy/core/core.gyp:core_unittest_utils',
        '<(src)/syzygy/testing/testing.gyp:testing_lib',
        '<(src)/syzygy/trace/agent_logger/agent_logger.gyp:agent_logger',
        '<(src)/syzygy/trace/agent_logger/agent_logger.gyp:agent_logger_lib',
        '<(src)/testing/gmock.gyp:gmock',
        '<(src)/testing/gtest.gyp:gtest',
       ],
      'msvs_settings': {
        'VCLinkerTool': {
          # Disable support for large address spaces.
          'LargeAddressAware': 1,
        },
      },
    },
    {
      'target_name': 'syzyasan_hp_lib',
      'type': 'static_library',
      'sources': [
        'hp_crt_interceptors.cc',
        'hp_crt_interceptors.h',
        'hot_patching_asan_runtime.cc',
        'hot_patching_asan_runtime.h',
      ],
      'dependencies': [
        'syzyasan_rtl_lib',
        '<(src)/syzygy/trace/client/client.gyp:rpc_client_lib',
        '<(src)/syzygy/trace/common/common.gyp:trace_common_lib',
        '<(src)/syzygy/trace/rpc/rpc.gyp:logger_rpc_lib',
        '<(src)/syzygy/trace/protocol/protocol.gyp:protocol_lib',
      ],
    },
    {
      'target_name': 'syzyasan_hp',
      'type': 'loadable_module',
      'includes': [
        '../agent.gypi',
        'agent_link_settings.gypi',
      ],
      'sources': [
        'syzyasan_hp.def',
        'syzyasan_hp.cc',
        'syzyasan_hp.rc',
      ],
      'dependencies': [
        'syzyasan_hp_lib',
        '<(src)/syzygy/agent/common/common.gyp:agent_common_lib',
        '<(src)/syzygy/common/common.gyp:common_lib',
        '<(src)/syzygy/core/core.gyp:core_lib',
        '<(src)/syzygy/version/version.gyp:syzygy_version',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          # This module should delay load nothing.
          'DelayLoadDLLs=': [
          ],
        },
      },
    },
    {
      'target_name': 'syzyasan_hp_unittests',
      'type': 'executable',
      'sources': [
        'hot_patching_asan_runtime_unittest.cc',
        '<(src)/base/test/run_all_unittests.cc',
      ],
      'dependencies': [
        'syzyasan_hp_lib',
        'syzyasan_hp',
        'syzyasan_rtl_unittest_utils',
        '<(src)/base/base.gyp:base',
        '<(src)/base/base.gyp:test_support_base',
        '<(src)/syzygy/agent/common/common.gyp:agent_common_lib',
        '<(src)/syzygy/common/common.gyp:common_lib',
        '<(src)/syzygy/core/core.gyp:core_unittest_utils',
        '<(src)/syzygy/instrument/instrument.gyp:instrument_lib',
        '<(src)/syzygy/pe/pe.gyp:pe_unittest_utils',
        '<(src)/syzygy/pe/pe.gyp:test_dll',
        '<(src)/syzygy/testing/testing.gyp:testing_lib',
        '<(src)/testing/gmock.gyp:gmock',
        '<(src)/testing/gtest.gyp:gtest',
       ],
      'msvs_settings': {
        'VCLinkerTool': {
          # Disable support for large address spaces.
          'LargeAddressAware': 1,
        },
      },
    },
    {
      'target_name': 'syzyasan_dyn',
      'type': 'loadable_module',
      'includes': [
        '../agent.gypi',
        'agent_link_settings.gypi',
      ],
      'sources': [
        # This file must have a .def extension in order for GYP to
        # automatically configure it as the ModuleDefinitionFile
        # (we usually suffix generated files with .gen).
        'gen/system_interceptors_dyn.def',
        'syzyasan_dyn.cc',
        'syzyasan_dyn.rc',
      ],
      'dependencies': [
        'syzyasan_rtl_lib',
        '<(src)/syzygy/agent/common/common.gyp:agent_common_lib',
        '<(src)/syzygy/common/common.gyp:common_lib',
        '<(src)/syzygy/core/core.gyp:core_lib',
        '<(src)/syzygy/version/version.gyp:syzygy_version',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          # This module should delay load nothing.
          'DelayLoadDLLs=': [
          ],
        },
      },
    },
  ],
}
