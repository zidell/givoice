//! Desktop SAPI recognition. This is not the Win+H online voice typing service.
use std::{
    path::Path,
    sync::atomic::{AtomicBool, Ordering},
    time::{Duration, Instant},
};
use windows::{
    core::{w, IUnknown, Interface, PCWSTR, PWSTR},
    Win32::{
        Globalization::{GetUserDefaultLangID, LocaleNameToLCID, LOCALE_ALLOW_NEUTRAL_NAMES},
        Media::Speech::*,
        System::Com::{
            CoCreateInstance, CoInitializeEx, CoTaskMemFree, CoUninitialize, CLSCTX_ALL,
            COINIT_MULTITHREADED,
        },
    },
};

struct Apartment;
impl Apartment {
    fn new() -> windows::core::Result<Self> {
        unsafe {
            CoInitializeEx(None, COINIT_MULTITHREADED).ok()?;
        }
        Ok(Self)
    }
}
impl Drop for Apartment {
    fn drop(&mut self) {
        unsafe {
            CoUninitialize();
        }
    }
}

unsafe fn owned_string(value: PWSTR) -> String {
    let text = value.to_string().unwrap_or_default();
    CoTaskMemFree(Some(value.0.cast()));
    text
}

fn wide(value: &str) -> Vec<u16> {
    value.encode_utf16().chain(Some(0)).collect()
}

unsafe fn recognizer(language: &str) -> Result<ISpRecognizer, String> {
    let name = wide(language);
    let locale = LocaleNameToLCID(PCWSTR(name.as_ptr()), LOCALE_ALLOW_NEUTRAL_NAMES);
    if locale == 0 {
        return Err(format!(
            "시스템 음성 인식이 언어 '{language}'를 지원하지 않습니다. 다른 엔진을 선택해 주세요."
        ));
    }
    let locale = preferred_locale(locale, GetUserDefaultLangID());
    let category: ISpObjectTokenCategory =
        CoCreateInstance(&SpObjectTokenCategory, None, CLSCTX_ALL).map_err(error)?;
    category.SetId(SPCAT_RECOGNIZERS, false).map_err(error)?;
    let tokens = category
        .EnumTokens(PCWSTR::null(), PCWSTR::null())
        .map_err(error)?;
    let mut count = 0;
    tokens.GetCount(&mut count).map_err(error)?;
    let mut matched = None;
    for index in 0..count {
        let Ok(token) = tokens.Item(index) else {
            continue;
        };
        let Ok(attributes) = token.OpenKey(w!("Attributes")) else {
            continue;
        };
        let Ok(value) = attributes.GetStringValue(w!("Language")) else {
            continue;
        };
        let languages = owned_string(value);
        let ids: Vec<u32> = languages
            .split(';')
            .filter_map(|id| u32::from_str_radix(id.trim(), 16).ok())
            .collect();
        if ids.contains(&(locale & 0xffff)) {
            matched = Some(token);
            break;
        }
        if matched.is_none() && ids.iter().any(|id| id & 0x3ff == locale & 0x3ff) {
            matched = Some(token);
        }
    }
    let Some(token) = matched else {
        let reason: String = if matches!(locale & 0x3ff, 0x09 | 0x0c | 0x07 | 0x11 | 0x04 | 0x0a) {
            "사용 가능한 데스크톱 음성 인식 엔진이 없습니다.\n‘언어 설정’에서 음성 기능 설치 여부를 확인한 뒤 ‘다시 확인’을 누르세요.\n설치 후에도 없으면 다른 엔진을 선택해 주세요.".into()
        } else if locale & 0x3ff == 0x12 {
            "현재 Givoice 시스템 엔진으로 한국어를 사용할 수 없습니다.\nWindows의 Win+H는 한국어를 지원하지만, Givoice는 아직 이 받아쓰기와 연결하지 않습니다.\nOpenAI / ElevenLabs / Groq를 선택해 주세요.".into()
        } else {
            "현재 Givoice에 연결된 데스크톱 엔진으로 이 언어를 사용할 수 없습니다.\nWindows 받아쓰기(Win+H)와 지원 범위가 다릅니다.\nOpenAI / ElevenLabs / Groq를 선택해 주세요.".into()
        };
        return Err(format!("'{language}': {reason}"));
    };
    let reco: ISpRecognizer =
        CoCreateInstance(&SpInprocRecognizer, None, CLSCTX_ALL).map_err(error)?;
    reco.SetRecognizer(&token).map_err(error)?;
    Ok(reco)
}

fn preferred_locale(requested: u32, user: u16) -> u32 {
    if requested & 0xfc00 == 0 && requested & 0x3ff == u32::from(user) & 0x3ff {
        u32::from(user)
    } else {
        requested
    }
}

struct ActiveRecognizer<'a>(&'a ISpRecognizer);
impl Drop for ActiveRecognizer<'_> {
    fn drop(&mut self) {
        unsafe {
            let _ = self.0.SetRecoState(SPRST_INACTIVE_WITH_PURGE);
        }
    }
}

fn error(error: windows::core::Error) -> String {
    format!("Windows 시스템 음성 인식을 사용할 수 없습니다 ({:#x}). 언어 설정과 데스크톱 음성 인식 엔진을 확인하거나 다른 엔진을 선택해 주세요.", error.code().0)
}

pub fn check(language: &str) -> Result<String, String> {
    let _apartment = Apartment::new().map_err(error)?;
    unsafe {
        let reco = recognizer(language)?;
        let context = reco.CreateRecoContext().map_err(error)?;
        let grammar = context.CreateGrammar(1).map_err(error)?;
        grammar
            .LoadDictation(PCWSTR::null(), SPLO_STATIC)
            .map_err(error)?;
    }
    Ok("사용 가능 · 설치된 Windows 데스크톱 엔진 · 오프라인 (Win+H와 다름)".into())
}

// SAPI transfers ownership of an event's lParam to the caller.
struct Event(SPEVENT);
impl Drop for Event {
    fn drop(&mut self) {
        unsafe {
            if self.0.lParam.0 == 0 {
                return;
            }
            match (self.0._bitfield as u32 >> 16) as i32 {
                1 | 2 => {
                    drop(IUnknown::from_raw(self.0.lParam.0 as *mut _));
                }
                3 | 4 => CoTaskMemFree(Some(self.0.lParam.0 as *const _)),
                _ => {}
            }
        }
    }
}

pub fn transcribe(wav: &Path, language: &str, cancelled: &AtomicBool) -> Result<String, String> {
    let _apartment = Apartment::new().map_err(error)?;
    unsafe {
        let reco = recognizer(language)?;
        let stream: ISpStream = CoCreateInstance(&SpStream, None, CLSCTX_ALL).map_err(error)?;
        let path = wide(&wav.to_string_lossy());
        stream
            .BindToFile(PCWSTR(path.as_ptr()), SPFM_OPEN_READONLY, None, None, 0)
            .map_err(error)?;
        reco.SetInput(&stream, true).map_err(error)?;
        let context = reco.CreateRecoContext().map_err(error)?;
        context.SetNotifyWin32Event().map_err(error)?;
        let interest = (1u64 << SPEI_RECOGNITION.0)
            | (1u64 << SPEI_END_SR_STREAM.0)
            | (1u64 << 30)
            | (1u64 << 33);
        context.SetInterest(interest, interest).map_err(error)?;
        let grammar = context.CreateGrammar(1).map_err(error)?;
        grammar
            .LoadDictation(PCWSTR::null(), SPLO_STATIC)
            .map_err(error)?;
        grammar.SetDictationState(SPRS_ACTIVE).map_err(error)?;
        reco.SetRecoState(SPRST_ACTIVE_ALWAYS).map_err(error)?;
        let _active = ActiveRecognizer(&reco);
        let reader = hound::WavReader::open(wav).map_err(|e| e.to_string())?;
        let deadline = Instant::now()
            + Duration::from_secs(
                u64::from(reader.duration() / reader.spec().sample_rate) * 2 + 60,
            );
        let mut results = Vec::new();
        loop {
            if cancelled.load(Ordering::Relaxed) {
                return Ok(String::new());
            }
            if Instant::now() > deadline {
                return Err("시스템 음성 인식 응답 시간이 초과되었습니다. 다른 엔진을 선택하거나 녹음을 짧게 나눠 주세요.".into());
            }
            let _ = context.WaitForNotifyEvent(100);
            loop {
                let mut raw = SPEVENT::default();
                let mut fetched = 0;
                context
                    .GetEvents(1, &mut raw, &mut fetched)
                    .map_err(error)?;
                if fetched == 0 {
                    break;
                }
                let event = Event(raw);
                let id = event.0._bitfield as u32 & 0xffff;
                if id == SPEI_RECOGNITION.0 as u32 {
                    if event.0.lParam.0 == 0 {
                        return Err("시스템 음성 인식 결과가 비어 있습니다.".into());
                    }
                    let object =
                        std::mem::ManuallyDrop::new(IUnknown::from_raw(event.0.lParam.0 as *mut _));
                    let result: ISpRecoResult = object.cast().map_err(error)?;
                    let mut text = PWSTR::null();
                    result
                        .GetText(0, u32::MAX, true, &mut text, None)
                        .map_err(error)?;
                    results.push(owned_string(text));
                } else if id == SPEI_END_SR_STREAM.0 as u32 {
                    if (event.0.lParam.0 as i32) < 0 {
                        return Err("시스템 음성 인식이 녹음 파일을 처리하지 못했습니다.".into());
                    }
                    return Ok(results.join(" "));
                }
            }
        }
    }
}

#[cfg(test)]
mod tests {
    #[test]
    fn neutral_language_prefers_user_region_without_overriding_explicit_region() {
        assert_eq!(super::preferred_locale(0x09, 0x809), 0x809);
        assert_eq!(super::preferred_locale(0x409, 0x809), 0x409);
        assert_eq!(super::preferred_locale(0x09, 0x412), 0x09);
        assert_eq!(super::preferred_locale(0x04, 0x404), 0x404);
    }

    #[test]
    #[ignore = "Requires an installed English SAPI engine and GIVOICE_SPEECH_TEST_WAV"]
    fn transcribes_spoken_fixture_and_honors_cancellation() {
        let path = std::path::PathBuf::from(
            std::env::var_os("GIVOICE_SPEECH_TEST_WAV").expect("fixture path"),
        );
        let cancelled = std::sync::atomic::AtomicBool::new(false);
        let text = super::transcribe(&path, "en", &cancelled).unwrap();
        assert!(text.to_lowercase().contains("hello"), "{text}");
        cancelled.store(true, std::sync::atomic::Ordering::Relaxed);
        assert!(super::transcribe(&path, "en", &cancelled)
            .unwrap()
            .is_empty());
        assert!(
            path.exists(),
            "recognition must preserve the original recording"
        );
    }
}
