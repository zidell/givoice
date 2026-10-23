//! A parked worker and an open output device, rather than PlaySound's cold
//! playback path on every shortcut. No silent buffers or polling while idle.
use std::{mem, ptr, sync::mpsc};
use windows_sys::Win32::{
    Foundation::{CloseHandle, HANDLE, HWND, WAIT_OBJECT_0},
    Media::Audio::*,
    System::Threading::{
        CreateEventW, GetCurrentThread, ResetEvent, SetThreadPriority, WaitForSingleObject,
        THREAD_PRIORITY_ABOVE_NORMAL,
    },
    UI::WindowsAndMessaging::PostMessageW,
};

pub struct Chime(mpsc::Sender<Request>);
struct Request {
    volume: u16,
    window: isize,
    message: u32,
    generation: u64,
}

impl Chime {
    pub fn new(volume: u16) -> Self {
        let (sender, receiver) = mpsc::channel::<Request>();
        std::thread::spawn(move || {
            // Only the tiny, usually parked cue worker gets a scheduling boost.
            // Recording, transcription and the rest of the process stay normal.
            unsafe {
                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
            }
            let mut output = Output::open().ok();
            let mut cached_volume = volume;
            let mut sound = crate::ui::recording_start_sound(volume);
            while let Ok(mut request) = receiver.recv() {
                // Rapid stop/start must not build a backlog of old cues.
                for newer in receiver.try_iter() {
                    request = newer;
                }
                if cached_volume != request.volume {
                    sound = crate::ui::recording_start_sound(request.volume);
                    cached_volume = request.volume;
                }
                if output.is_none() {
                    output = Output::open().ok();
                }
                if let Some(device) = output.as_mut() {
                    if let Err(error) = device.play(&mut sound[44..]) {
                        crate::debug_log::log(|| format!("start chime failed: {error}"));
                        output = None;
                    }
                }
                unsafe {
                    PostMessageW(
                        request.window as HWND,
                        request.message,
                        request.generation as usize,
                        0,
                    );
                }
            }
        });
        Self(sender)
    }

    pub fn play(&self, volume: u16, window: HWND, message: u32, generation: u64) {
        let request = Request {
            volume,
            window: window as isize,
            message,
            generation,
        };
        if self.0.send(request).is_err() {
            // Preserve mute-after-cue if the worker is unavailable.
            unsafe {
                PostMessageW(window, message, generation as usize, 0);
            }
        }
    }
}

struct Output {
    device: HWAVEOUT,
    event: HANDLE,
}

// WAVEHDR is packed in the Windows bindings. Align its flags explicitly for
// the volatile read after the driver signals completion.
#[repr(C, align(8))]
struct Header(WAVEHDR);

impl Output {
    fn open() -> Result<Self, u32> {
        unsafe {
            let event = CreateEventW(ptr::null(), 0, 0, ptr::null());
            if event.is_null() {
                return Err(1);
            }
            let wav = include_bytes!("../../../assets/recording-start.wav");
            let channels = u16::from_le_bytes(wav[22..24].try_into().unwrap());
            let rate = u32::from_le_bytes(wav[24..28].try_into().unwrap());
            let bits = u16::from_le_bytes(wav[34..36].try_into().unwrap());
            let align = channels * bits / 8;
            let format = WAVEFORMATEX {
                wFormatTag: 1,
                nChannels: channels,
                nSamplesPerSec: rate,
                nAvgBytesPerSec: rate * u32::from(align),
                nBlockAlign: align,
                wBitsPerSample: bits,
                cbSize: 0,
            };
            let mut device = ptr::null_mut();
            let result = waveOutOpen(
                &mut device,
                WAVE_MAPPER,
                &format,
                event as usize,
                0,
                CALLBACK_EVENT,
            );
            if result != 0 {
                CloseHandle(event);
                return Err(result);
            }
            Ok(Self { device, event })
        }
    }

    fn play(&mut self, pcm: &mut [u8]) -> Result<(), u32> {
        unsafe {
            let mut header = Header(WAVEHDR {
                lpData: pcm.as_mut_ptr().cast(),
                dwBufferLength: pcm.len() as u32,
                ..mem::zeroed()
            });
            let size = mem::size_of::<WAVEHDR>() as u32;
            let result = waveOutPrepareHeader(self.device, &mut header.0, size);
            if result != 0 {
                return Err(result);
            }
            ResetEvent(self.event);
            let mut result = waveOutWrite(self.device, &mut header.0, size);
            if result == 0 {
                loop {
                    if WaitForSingleObject(self.event, 2000) != WAIT_OBJECT_0 {
                        result = 1;
                        break;
                    }
                    if ptr::read_volatile(ptr::addr_of!(header.0.dwFlags)) & WHDR_DONE != 0 {
                        break;
                    }
                }
            }
            // Reset returns buffers before their backing memory is released.
            if result != 0 {
                waveOutReset(self.device);
            }
            waveOutUnprepareHeader(self.device, &mut header.0, size);
            if result == 0 {
                Ok(())
            } else {
                Err(result)
            }
        }
    }
}

impl Drop for Output {
    fn drop(&mut self) {
        unsafe {
            waveOutReset(self.device);
            waveOutClose(self.device);
            CloseHandle(self.event);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn embedded_cue_has_the_pcm_layout_used_by_output() {
        let wav = include_bytes!("../../../assets/recording-start.wav");
        assert_eq!(&wav[..4], b"RIFF");
        assert_eq!(&wav[8..16], b"WAVEfmt ");
        assert_eq!(u32::from_le_bytes(wav[16..20].try_into().unwrap()), 16);
        assert_eq!(u16::from_le_bytes(wav[20..22].try_into().unwrap()), 1);
        assert_eq!(u16::from_le_bytes(wav[34..36].try_into().unwrap()), 16);
        assert_eq!(&wav[36..40], b"data");
        assert_eq!(
            u32::from_le_bytes(wav[40..44].try_into().unwrap()) as usize,
            wav.len() - 44
        );
        let reader = hound::WavReader::new(std::io::Cursor::new(wav)).unwrap();
        assert_eq!(reader.spec().sample_rate, 24_000);
        assert_eq!(reader.spec().channels, 1);
    }

    #[test]
    #[ignore = "requires a Windows output device; plays two quiet cues"]
    fn reuses_output_and_waits_for_each_buffer_to_finish() {
        let mut output = Output::open().expect("open audio output");
        let mut sound = crate::ui::recording_start_sound(10);
        for _ in 0..2 {
            let started = std::time::Instant::now();
            output.play(&mut sound[44..]).expect("play and drain cue");
            eprintln!("cue submitted and drained in {:?}", started.elapsed());
        }
    }
}
