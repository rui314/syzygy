; Copyright 2015 Google Inc. All Rights Reserved.
;
; Licensed under the Apache License, Version 2.0 (the "License");
; you may not use this file except in compliance with the License.
; You may obtain a copy of the License at
;
;     http://www.apache.org/licenses/LICENSE-2.0
;
; Unless required by applicable law or agreed to in writing, software
; distributed under the License is distributed on an "AS IS" BASIS,
; WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
; See the License for the specific language governing permissions and
; limitations under the License.

; This file is generated by generate_memory_interceptors.py, DO NOT MODIFY.

.386
.MODEL FLAT, C

.CODE

; Declare the tail function all the stubs direct to.
EXTERN C asan_redirect_tail:PROC

PUBLIC asan_redirect_1_byte_read_access
PUBLIC asan_redirect_1_byte_write_access
PUBLIC asan_redirect_2_byte_read_access
PUBLIC asan_redirect_2_byte_write_access
PUBLIC asan_redirect_4_byte_read_access
PUBLIC asan_redirect_4_byte_write_access
PUBLIC asan_redirect_8_byte_read_access
PUBLIC asan_redirect_8_byte_write_access
PUBLIC asan_redirect_10_byte_read_access
PUBLIC asan_redirect_10_byte_write_access
PUBLIC asan_redirect_16_byte_read_access
PUBLIC asan_redirect_16_byte_write_access
PUBLIC asan_redirect_32_byte_read_access
PUBLIC asan_redirect_32_byte_write_access
PUBLIC asan_redirect_1_byte_read_access_no_flags
PUBLIC asan_redirect_1_byte_write_access_no_flags
PUBLIC asan_redirect_2_byte_read_access_no_flags
PUBLIC asan_redirect_2_byte_write_access_no_flags
PUBLIC asan_redirect_4_byte_read_access_no_flags
PUBLIC asan_redirect_4_byte_write_access_no_flags
PUBLIC asan_redirect_8_byte_read_access_no_flags
PUBLIC asan_redirect_8_byte_write_access_no_flags
PUBLIC asan_redirect_10_byte_read_access_no_flags
PUBLIC asan_redirect_10_byte_write_access_no_flags
PUBLIC asan_redirect_16_byte_read_access_no_flags
PUBLIC asan_redirect_16_byte_write_access_no_flags
PUBLIC asan_redirect_32_byte_read_access_no_flags
PUBLIC asan_redirect_32_byte_write_access_no_flags
PUBLIC asan_redirect_repz_4_byte_cmps_access
PUBLIC asan_redirect_repz_2_byte_cmps_access
PUBLIC asan_redirect_repz_1_byte_cmps_access
PUBLIC asan_redirect_4_byte_cmps_access
PUBLIC asan_redirect_2_byte_cmps_access
PUBLIC asan_redirect_1_byte_cmps_access
PUBLIC asan_redirect_repz_4_byte_movs_access
PUBLIC asan_redirect_repz_2_byte_movs_access
PUBLIC asan_redirect_repz_1_byte_movs_access
PUBLIC asan_redirect_4_byte_movs_access
PUBLIC asan_redirect_2_byte_movs_access
PUBLIC asan_redirect_1_byte_movs_access
PUBLIC asan_redirect_repz_4_byte_stos_access
PUBLIC asan_redirect_repz_2_byte_stos_access
PUBLIC asan_redirect_repz_1_byte_stos_access
PUBLIC asan_redirect_4_byte_stos_access
PUBLIC asan_redirect_2_byte_stos_access
PUBLIC asan_redirect_1_byte_stos_access

; Declare a single top-level function to prevent identical code folding from
; folding the redirectors into one. Each redirector simply calls through to
; the tail function. This allows the tail function to trivially compute the
; redirector's address, which is used to identify the invoked redirector.
asan_redirectors PROC

asan_redirect_1_byte_read_access LABEL PROC
  call asan_redirect_tail
asan_redirect_1_byte_write_access LABEL PROC
  call asan_redirect_tail
asan_redirect_2_byte_read_access LABEL PROC
  call asan_redirect_tail
asan_redirect_2_byte_write_access LABEL PROC
  call asan_redirect_tail
asan_redirect_4_byte_read_access LABEL PROC
  call asan_redirect_tail
asan_redirect_4_byte_write_access LABEL PROC
  call asan_redirect_tail
asan_redirect_8_byte_read_access LABEL PROC
  call asan_redirect_tail
asan_redirect_8_byte_write_access LABEL PROC
  call asan_redirect_tail
asan_redirect_10_byte_read_access LABEL PROC
  call asan_redirect_tail
asan_redirect_10_byte_write_access LABEL PROC
  call asan_redirect_tail
asan_redirect_16_byte_read_access LABEL PROC
  call asan_redirect_tail
asan_redirect_16_byte_write_access LABEL PROC
  call asan_redirect_tail
asan_redirect_32_byte_read_access LABEL PROC
  call asan_redirect_tail
asan_redirect_32_byte_write_access LABEL PROC
  call asan_redirect_tail
asan_redirect_1_byte_read_access_no_flags LABEL PROC
  call asan_redirect_tail
asan_redirect_1_byte_write_access_no_flags LABEL PROC
  call asan_redirect_tail
asan_redirect_2_byte_read_access_no_flags LABEL PROC
  call asan_redirect_tail
asan_redirect_2_byte_write_access_no_flags LABEL PROC
  call asan_redirect_tail
asan_redirect_4_byte_read_access_no_flags LABEL PROC
  call asan_redirect_tail
asan_redirect_4_byte_write_access_no_flags LABEL PROC
  call asan_redirect_tail
asan_redirect_8_byte_read_access_no_flags LABEL PROC
  call asan_redirect_tail
asan_redirect_8_byte_write_access_no_flags LABEL PROC
  call asan_redirect_tail
asan_redirect_10_byte_read_access_no_flags LABEL PROC
  call asan_redirect_tail
asan_redirect_10_byte_write_access_no_flags LABEL PROC
  call asan_redirect_tail
asan_redirect_16_byte_read_access_no_flags LABEL PROC
  call asan_redirect_tail
asan_redirect_16_byte_write_access_no_flags LABEL PROC
  call asan_redirect_tail
asan_redirect_32_byte_read_access_no_flags LABEL PROC
  call asan_redirect_tail
asan_redirect_32_byte_write_access_no_flags LABEL PROC
  call asan_redirect_tail
asan_redirect_repz_4_byte_cmps_access LABEL PROC
  call asan_redirect_tail
asan_redirect_repz_2_byte_cmps_access LABEL PROC
  call asan_redirect_tail
asan_redirect_repz_1_byte_cmps_access LABEL PROC
  call asan_redirect_tail
asan_redirect_4_byte_cmps_access LABEL PROC
  call asan_redirect_tail
asan_redirect_2_byte_cmps_access LABEL PROC
  call asan_redirect_tail
asan_redirect_1_byte_cmps_access LABEL PROC
  call asan_redirect_tail
asan_redirect_repz_4_byte_movs_access LABEL PROC
  call asan_redirect_tail
asan_redirect_repz_2_byte_movs_access LABEL PROC
  call asan_redirect_tail
asan_redirect_repz_1_byte_movs_access LABEL PROC
  call asan_redirect_tail
asan_redirect_4_byte_movs_access LABEL PROC
  call asan_redirect_tail
asan_redirect_2_byte_movs_access LABEL PROC
  call asan_redirect_tail
asan_redirect_1_byte_movs_access LABEL PROC
  call asan_redirect_tail
asan_redirect_repz_4_byte_stos_access LABEL PROC
  call asan_redirect_tail
asan_redirect_repz_2_byte_stos_access LABEL PROC
  call asan_redirect_tail
asan_redirect_repz_1_byte_stos_access LABEL PROC
  call asan_redirect_tail
asan_redirect_4_byte_stos_access LABEL PROC
  call asan_redirect_tail
asan_redirect_2_byte_stos_access LABEL PROC
  call asan_redirect_tail
asan_redirect_1_byte_stos_access LABEL PROC
  call asan_redirect_tail
asan_redirectors ENDP

END
