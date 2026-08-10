use std::panic::{catch_unwind, AssertUnwindSafe};

use blake3::hazmat::HasherExt;

const KEY_BYTES: usize = 32;
const DIGEST_BYTES: usize = 32;
const CHUNK_BYTES: usize = 1024;

fn read_key(ptr: *const u8) -> Option<[u8; KEY_BYTES]> {
    if ptr.is_null() {
        return None;
    }
    let mut key = [0u8; KEY_BYTES];
    // SAFETY: the C ABI requires a readable 32-byte key when the pointer is
    // non-null. The copy does not outlive the caller's buffer.
    unsafe { std::ptr::copy_nonoverlapping(ptr, key.as_mut_ptr(), KEY_BYTES) };
    Some(key)
}

fn write_digest(ptr: *mut u8, digest: &[u8; DIGEST_BYTES]) -> bool {
    if ptr.is_null() {
        return false;
    }
    // SAFETY: the C ABI requires a writable 32-byte output buffer when the
    // pointer is non-null.
    unsafe { std::ptr::copy_nonoverlapping(digest.as_ptr(), ptr, DIGEST_BYTES) };
    true
}

#[no_mangle]
pub extern "C" fn pearl_blake3_hash(
    data: *const u8,
    data_len: usize,
    output: *mut u8,
) -> i32 {
    catch_unwind(AssertUnwindSafe(|| {
        if data_len != 0 && data.is_null() {
            return -1;
        }
        let bytes = if data_len == 0 {
            &[][..]
        } else {
            // SAFETY: the C ABI requires `data_len` readable bytes.
            unsafe { std::slice::from_raw_parts(data, data_len) }
        };
        let digest = *blake3::hash(bytes).as_bytes();
        if write_digest(output, &digest) { 0 } else { -1 }
    }))
    .unwrap_or(-2)
}

#[no_mangle]
pub extern "C" fn pearl_blake3_hash_keyed(
    key: *const u8,
    data: *const u8,
    data_len: usize,
    output: *mut u8,
) -> i32 {
    catch_unwind(AssertUnwindSafe(|| {
        let Some(key) = read_key(key) else { return -1 };
        if data_len != 0 && data.is_null() {
            return -1;
        }
        let bytes = if data_len == 0 {
            &[][..]
        } else {
            // SAFETY: the C ABI requires `data_len` readable bytes.
            unsafe { std::slice::from_raw_parts(data, data_len) }
        };
        let digest = *blake3::keyed_hash(&key, bytes).as_bytes();
        if write_digest(output, &digest) { 0 } else { -1 }
    }))
    .unwrap_or(-2)
}

#[no_mangle]
pub extern "C" fn pearl_blake3_chunk_cv(
    key: *const u8,
    data: *const u8,
    data_len: usize,
    chunk_index: u64,
    output: *mut u8,
) -> i32 {
    catch_unwind(AssertUnwindSafe(|| {
        let Some(key) = read_key(key) else { return -1 };
        if data_len > CHUNK_BYTES || (data_len != 0 && data.is_null()) {
            return -1;
        }
        let bytes = if data_len == 0 {
            &[][..]
        } else {
            // SAFETY: the C ABI requires `data_len` readable bytes.
            unsafe { std::slice::from_raw_parts(data, data_len) }
        };
        let mut hasher = blake3::Hasher::new_keyed(&key);
        hasher.set_input_offset(chunk_index.saturating_mul(CHUNK_BYTES as u64));
        hasher.update(bytes);
        let digest = hasher.finalize_non_root();
        if write_digest(output, &digest) { 0 } else { -1 }
    }))
    .unwrap_or(-2)
}

#[no_mangle]
pub extern "C" fn pearl_blake3_parent_cv(
    key: *const u8,
    left: *const u8,
    right: *const u8,
    root: bool,
    output: *mut u8,
) -> i32 {
    catch_unwind(AssertUnwindSafe(|| {
        let Some(key) = read_key(key) else { return -1 };
        if left.is_null() || right.is_null() {
            return -1;
        }
        let mut left_digest = [0u8; DIGEST_BYTES];
        let mut right_digest = [0u8; DIGEST_BYTES];
        // SAFETY: the C ABI requires readable 32-byte child CVs.
        unsafe {
            std::ptr::copy_nonoverlapping(left, left_digest.as_mut_ptr(), DIGEST_BYTES);
            std::ptr::copy_nonoverlapping(right, right_digest.as_mut_ptr(), DIGEST_BYTES);
        }
        let mode = blake3::hazmat::Mode::KeyedHash(&key);
        let digest = if root {
            *blake3::hazmat::merge_subtrees_root(&left_digest, &right_digest, mode).as_bytes()
        } else {
            blake3::hazmat::merge_subtrees_non_root(&left_digest, &right_digest, mode)
        };
        if write_digest(output, &digest) { 0 } else { -1 }
    }))
    .unwrap_or(-2)
}
