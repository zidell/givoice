//! 로그인 시 자동 실행. 설정 파일에 따로 두지 않고 HKCU Run 값 자체를 상태로 쓴다.

use std::ptr;
use windows_sys::Win32::{
    Foundation::{ERROR_FILE_NOT_FOUND, ERROR_SUCCESS},
    System::Registry::{
        RegCloseKey, RegCreateKeyExW, RegDeleteValueW, RegQueryValueExW, RegSetValueExW, HKEY,
        HKEY_CURRENT_USER, KEY_QUERY_VALUE, KEY_SET_VALUE, REG_OPTION_NON_VOLATILE, REG_SZ,
    },
};

const RUN_KEY: &str = r"Software\Microsoft\Windows\CurrentVersion\Run";
const VALUE: &str = "Givoice";

fn wide(text: &str) -> Vec<u16> {
    text.encode_utf16().chain(Some(0)).collect()
}

unsafe fn open(access: u32) -> Result<HKEY, String> {
    let mut key: HKEY = ptr::null_mut();
    let status = RegCreateKeyExW(
        HKEY_CURRENT_USER,
        wide(RUN_KEY).as_ptr(),
        0,
        ptr::null(),
        REG_OPTION_NON_VOLATILE,
        access,
        ptr::null(),
        &mut key,
        ptr::null_mut(),
    );
    if status == ERROR_SUCCESS {
        Ok(key)
    } else {
        Err(format!("자동 실행 레지스트리를 열 수 없습니다 (오류 {status})"))
    }
}

pub fn enabled() -> bool {
    unsafe {
        let Ok(key) = open(KEY_QUERY_VALUE) else {
            return false;
        };
        let status = RegQueryValueExW(
            key,
            wide(VALUE).as_ptr(),
            ptr::null(),
            ptr::null_mut(),
            ptr::null_mut(),
            ptr::null_mut(),
        );
        RegCloseKey(key);
        status == ERROR_SUCCESS
    }
}

pub fn set(enabled: bool) -> Result<(), String> {
    unsafe {
        let key = open(KEY_SET_VALUE)?;
        let status = if enabled {
            match std::env::current_exe() {
                // Windows 파일 이름에는 따옴표가 들어갈 수 없어 경로를 감싸기만 하면 된다.
                Ok(exe) => {
                    let command = wide(&format!("\"{}\"", exe.display()));
                    RegSetValueExW(
                        key,
                        wide(VALUE).as_ptr(),
                        0,
                        REG_SZ,
                        command.as_ptr().cast(),
                        (command.len() * 2) as u32,
                    )
                }
                Err(error) => {
                    RegCloseKey(key);
                    return Err(format!("실행 파일 경로를 찾을 수 없습니다: {error}"));
                }
            }
        } else {
            match RegDeleteValueW(key, wide(VALUE).as_ptr()) {
                ERROR_FILE_NOT_FOUND => ERROR_SUCCESS,
                status => status,
            }
        };
        RegCloseKey(key);
        if status == ERROR_SUCCESS {
            Ok(())
        } else {
            Err(format!("자동 실행 설정을 저장하지 못했습니다 (오류 {status})"))
        }
    }
}
