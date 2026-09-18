/**
 * @file App.tsx
 * @brief WiFi-HiFi Interactive Receiver Simulator & Development Workbench
 */

import React, { useState, useEffect, useRef } from 'react';
import {
  Volume2,
  VolumeX,
  Radio,
  Sliders,
  Cpu,
  Activity,
  Wifi,
  Shield,
  FileText,
  Play,
  Square,
  RefreshCw,
  Terminal,
  Zap,
  CheckCircle2,
  AlertTriangle
} from 'lucide-react';

interface AudioMetrics {
  packetsReceived: number;
  packetsLost: number;
  bufferFillPct: number;
  underruns: number;
  overruns: number;
  bitrateKbps: number;
  lossPct: number;
}

export default function App() {
  const [activeTab, setActiveTab] = useState<'simulator' | 'hardware' | 'protocol' | 'android'>('simulator');
  const [isPlaying, setIsPlaying] = useState(false);
  const [volume, setVolume] = useState(85);
  const [isMuted, setIsMuted] = useState(false);
  const [latencyMode, setLatencyMode] = useState<'LOW_LATENCY' | 'BALANCED' | 'STABLE'>('BALANCED');
  const [simulatedLossRate, setSimulatedLossRate] = useState(0.0);

  // Audio Context and Simulator state
  const audioCtxRef = useRef<AudioContext | null>(null);
  const oscRef = useRef<OscillatorNode | null>(null);
  const gainRef = useRef<GainNode | null>(null);
  const analyserRef = useRef<AnalyserNode | null>(null);
  const canvasRef = useRef<HTMLCanvasElement | null>(null);
  const animFrameRef = useRef<number | null>(null);

  const [metrics, setMetrics] = useState<AudioMetrics>({
    packetsReceived: 0,
    packetsLost: 0,
    bufferFillPct: 45.0,
    underruns: 0,
    overruns: 0,
    bitrateKbps: 1411.2,
    lossPct: 0.0,
  });

  const [lastPacketHex, setLastPacketHex] = useState<string>(
    '57 46 48 46 01 01 2A 04 68 01 00 00 E8 03 00 00 00 00 00 00 44 AC 00 00 02 10 01 00 E4 06 00 00'
  );

  // Initialize Web Audio API for test tone streaming
  const startStreaming = async () => {
    try {
      const ctx = new (window.AudioContext || (window as any).webkitAudioContext)({ sampleRate: 44100 });
      audioCtxRef.current = ctx;

      const osc = ctx.createOscillator();
      osc.type = 'sine';
      osc.frequency.setValueAtTime(440, ctx.currentTime);

      const gain = ctx.createGain();
      gain.gain.setValueAtTime(isMuted ? 0 : volume / 100 * 0.15, ctx.currentTime);

      const analyser = ctx.createAnalyser();
      analyser.fftSize = 256;

      osc.connect(gain);
      gain.connect(analyser);
      gain.connect(ctx.destination);

      osc.start();

      oscRef.current = osc;
      gainRef.current = gain;
      analyserRef.current = analyser;

      setIsPlaying(true);
    } catch (e) {
      console.error('AudioContext error:', e);
    }
  };

  const stopStreaming = () => {
    if (oscRef.current) {
      try {
        oscRef.current.stop();
        oscRef.current.disconnect();
      } catch (e) {}
      oscRef.current = null;
    }
    if (audioCtxRef.current) {
      audioCtxRef.current.close();
      audioCtxRef.current = null;
    }
    setIsPlaying(false);
  };

  // Synchronize gain with volume slider
  useEffect(() => {
    if (gainRef.current && audioCtxRef.current) {
      const targetGain = isMuted ? 0 : (volume / 100) * 0.15;
      gainRef.current.gain.setTargetAtTime(targetGain, audioCtxRef.current.currentTime, 0.05);
    }
  }, [volume, isMuted]);

  // Simulation tick for packet telemetry
  useEffect(() => {
    if (!isPlaying) return;

    let seq = 360;
    const interval = setInterval(() => {
      seq += 100; // 100 packets per second (10ms each)
      const isLost = Math.random() < simulatedLossRate;

      setMetrics((prev) => {
        const packetsRcvd = prev.packetsReceived + (isLost ? 0 : 100);
        const packetsLost = prev.packetsLost + (isLost ? 1 : 0);
        const total = packetsRcvd + packetsLost;
        const lossPct = total > 0 ? (packetsLost / total) * 100 : 0;
        const underruns = prev.underruns + (isLost ? 1 : 0);
        const baseFill = latencyMode === 'LOW_LATENCY' ? 25 : latencyMode === 'STABLE' ? 75 : 50;
        const jitteredFill = Math.min(100, Math.max(10, baseFill + (Math.random() * 8 - 4)));

        return {
          packetsReceived: packetsRcvd,
          packetsLost,
          bufferFillPct: parseFloat(jitteredFill.toFixed(1)),
          underruns,
          overruns: prev.overruns,
          bitrateKbps: 1411.2,
          lossPct: parseFloat(lossPct.toFixed(2)),
        };
      });

      // Update sample packet hex (Magic "WFHF" = 0x57, 0x46, 0x48, 0x46)
      const seqByte0 = (seq & 0xff).toString(16).padStart(2, '0').toUpperCase();
      const seqByte1 = ((seq >> 8) & 0xff).toString(16).padStart(2, '0').toUpperCase();
      setLastPacketHex(
        `57 46 48 46 01 01 2A 04 ${seqByte0} ${seqByte1} 00 00 E8 03 00 00 00 00 00 00 44 AC 00 00 02 10 01 00 E4 06 00 00`
      );
    }, 1000);

    return () => clearInterval(interval);
  }, [isPlaying, simulatedLossRate, latencyMode]);

  // Canvas waveform visualizer
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const render = () => {
      animFrameRef.current = requestAnimationFrame(render);
      const width = canvas.width;
      const height = canvas.height;

      ctx.fillStyle = '#0F1117';
      ctx.fillRect(0, 0, width, height);

      // Grid lines
      ctx.strokeStyle = '#1F2430';
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.moveTo(0, height / 2);
      ctx.lineTo(width, height / 2);
      ctx.stroke();

      if (!isPlaying || !analyserRef.current) {
        ctx.strokeStyle = '#2E3444';
        ctx.beginPath();
        ctx.moveTo(0, height / 2);
        ctx.lineTo(width, height / 2);
        ctx.stroke();
        return;
      }

      const bufferLength = analyserRef.current.frequencyBinCount;
      const dataArray = new Uint8Array(bufferLength);
      analyserRef.current.getByteTimeDomainData(dataArray);

      ctx.lineWidth = 2;
      ctx.strokeStyle = '#D4AF37'; // Gold
      ctx.beginPath();

      const sliceWidth = (width * 1.0) / bufferLength;
      let x = 0;

      for (let i = 0; i < bufferLength; i++) {
        const v = dataArray[i] / 128.0;
        const y = (v * height) / 2;

        if (i === 0) {
          ctx.moveTo(x, y);
        } else {
          ctx.lineTo(x, y);
        }
        x += sliceWidth;
      }

      ctx.lineTo(width, height / 2);
      ctx.stroke();
    };

    render();

    return () => {
      if (animFrameRef.current) cancelAnimationFrame(animFrameRef.current);
    };
  }, [isPlaying]);

  return (
    <div className="min-h-screen bg-[#0A0C10] text-[#F0F3FA] font-sans flex flex-col">
      {/* Top Header */}
      <header className="border-b border-[#202534] bg-[#0F1117] px-6 py-4 flex items-center justify-between">
        <div className="flex items-center space-x-3">
          <div className="w-9 h-9 rounded-lg bg-[#D4AF37] flex items-center justify-center text-black font-bold text-lg shadow-md">
            W
          </div>
          <div>
            <div className="flex items-center space-x-2">
              <h1 className="text-xl font-bold tracking-tight text-[#F0F3FA]">WiFi-HiFi</h1>
              <span className="text-xs px-2 py-0.5 rounded bg-[#202430] text-[#D4AF37] border border-[#3E3820] font-mono">
                ESP32-C3 + Android 16+
              </span>
            </div>
            <p className="text-xs text-[#8E98AC]">Production Lossless Linear PCM Wireless Audio Ecosystem</p>
          </div>
        </div>

        {/* Navigation Tabs */}
        <div className="flex bg-[#161922] p-1 rounded-lg border border-[#232838]">
          <button
            onClick={() => setActiveTab('simulator')}
            className={`px-3 py-1.5 rounded-md text-xs font-medium transition-all ${
              activeTab === 'simulator' ? 'bg-[#D4AF37] text-black shadow-sm font-semibold' : 'text-[#8E98AC] hover:text-[#F0F3FA]'
            }`}
          >
            Live Receiver
          </button>
          <button
            onClick={() => setActiveTab('hardware')}
            className={`px-3 py-1.5 rounded-md text-xs font-medium transition-all ${
              activeTab === 'hardware' ? 'bg-[#D4AF37] text-black shadow-sm font-semibold' : 'text-[#8E98AC] hover:text-[#F0F3FA]'
            }`}
          >
            Hardware & I2S
          </button>
          <button
            onClick={() => setActiveTab('protocol')}
            className={`px-3 py-1.5 rounded-md text-xs font-medium transition-all ${
              activeTab === 'protocol' ? 'bg-[#D4AF37] text-black shadow-sm font-semibold' : 'text-[#8E98AC] hover:text-[#F0F3FA]'
            }`}
          >
            Binary Protocol
          </button>
          <button
            onClick={() => setActiveTab('android')}
            className={`px-3 py-1.5 rounded-md text-xs font-medium transition-all ${
              activeTab === 'android' ? 'bg-[#D4AF37] text-black shadow-sm font-semibold' : 'text-[#8E98AC] hover:text-[#F0F3FA]'
            }`}
          >
            Android Sender
          </button>
        </div>
      </header>

      {/* Main Content Area */}
      <main className="flex-1 p-6 max-w-7xl mx-auto w-full">
        {activeTab === 'simulator' && (
          <div className="grid grid-cols-1 lg:grid-cols-3 gap-6">
            {/* Left Column: Stream Controls & Oscilloscope */}
            <div className="lg:col-span-2 space-y-6">
              {/* Receiver Status Banner */}
              <div className="bg-[#12151E] border border-[#202534] rounded-xl p-5 shadow-sm">
                <div className="flex items-center justify-between mb-4">
                  <div className="flex items-center space-x-3">
                    <div
                      className={`w-3 h-3 rounded-full ${
                        isPlaying ? 'bg-emerald-400 animate-pulse' : 'bg-[#4B5563]'
                      }`}
                    />
                    <div>
                      <h2 className="text-base font-semibold text-[#F0F3FA]">ESP32-C3 WiFi-HiFi Receiver</h2>
                      <p className="text-xs text-[#8E98AC] font-mono">wifi-hifi.local • 192.168.1.105:50005</p>
                    </div>
                  </div>

                  <div className="flex items-center space-x-2">
                    <span
                      className={`text-xs px-2.5 py-1 rounded font-semibold font-mono ${
                        isPlaying ? 'bg-emerald-950/60 text-emerald-400 border border-emerald-800/50' : 'bg-[#1C202C] text-[#8E98AC]'
                      }`}
                    >
                      {isPlaying ? 'STREAMING ACTIVE' : 'STANDBY'}
                    </span>
                  </div>
                </div>

                {/* Live Oscilloscope */}
                <div className="relative rounded-lg overflow-hidden border border-[#202534] bg-[#0F1117] mb-4">
                  <canvas ref={canvasRef} width={640} height={160} className="w-full h-40 block" />
                  <div className="absolute top-2 left-3 flex items-center space-x-2 text-[11px] font-mono text-[#8E98AC]">
                    <span>44.1 kHz</span>
                    <span>•</span>
                    <span>16-bit Stereo</span>
                    <span>•</span>
                    <span>Linear PCM</span>
                  </div>
                  <div className="absolute bottom-2 right-3 text-[11px] font-mono text-[#D4AF37]">
                    UDA1334A I2S Master Clock PLL Active
                  </div>
                </div>

                {/* Primary Action Button */}
                <div className="flex items-center space-x-4">
                  {isPlaying ? (
                    <button
                      onClick={stopStreaming}
                      className="flex-1 bg-rose-600 hover:bg-rose-500 text-white font-semibold py-3 px-4 rounded-lg flex items-center justify-center space-x-2 transition-colors shadow-lg shadow-rose-950/50"
                    >
                      <Square className="w-4 h-4 fill-current" />
                      <span>Stop Audio Transmission</span>
                    </button>
                  ) : (
                    <button
                      onClick={startStreaming}
                      className="flex-1 bg-[#D4AF37] hover:bg-[#E5C158] text-black font-semibold py-3 px-4 rounded-lg flex items-center justify-center space-x-2 transition-colors shadow-lg shadow-amber-950/40"
                    >
                      <Play className="w-4 h-4 fill-current" />
                      <span>Start Audio Stream (Test Tone)</span>
                    </button>
                  )}
                </div>
              </div>

              {/* Real-Time Metrics Bento */}
              <div className="grid grid-cols-2 sm:grid-cols-4 gap-4">
                <div className="bg-[#12151E] border border-[#202534] p-4 rounded-xl">
                  <p className="text-xs text-[#8E98AC] mb-1 font-medium">Throughput</p>
                  <p className="text-xl font-bold font-mono text-[#F0F3FA]">{isPlaying ? metrics.bitrateKbps : '0.0'} kbps</p>
                  <p className="text-[11px] text-[#8E98AC] mt-1">Lossless Uncompressed</p>
                </div>

                <div className="bg-[#12151E] border border-[#202534] p-4 rounded-xl">
                  <p className="text-xs text-[#8E98AC] mb-1 font-medium">Jitter Buffer Fill</p>
                  <p className="text-xl font-bold font-mono text-[#D4AF37]">{isPlaying ? `${metrics.bufferFillPct}%` : '0.0%'}</p>
                  <div className="w-full bg-[#1F2430] h-1.5 rounded-full mt-2 overflow-hidden">
                    <div
                      className="bg-[#D4AF37] h-full transition-all duration-300"
                      style={{ width: `${isPlaying ? metrics.bufferFillPct : 0}%` }}
                    />
                  </div>
                </div>

                <div className="bg-[#12151E] border border-[#202534] p-4 rounded-xl">
                  <p className="text-xs text-[#8E98AC] mb-1 font-medium">Packet Loss</p>
                  <p className={`text-xl font-bold font-mono ${metrics.lossPct > 0 ? 'text-amber-400' : 'text-emerald-400'}`}>
                    {metrics.lossPct}%
                  </p>
                  <p className="text-[11px] text-[#8E98AC] mt-1">{metrics.packetsLost} packets dropped</p>
                </div>

                <div className="bg-[#12151E] border border-[#202534] p-4 rounded-xl">
                  <p className="text-xs text-[#8E98AC] mb-1 font-medium">Underrun Events</p>
                  <p className={`text-xl font-bold font-mono ${metrics.underruns > 0 ? 'text-rose-400' : 'text-[#F0F3FA]'}`}>
                    {metrics.underruns}
                  </p>
                  <p className="text-[11px] text-[#8E98AC] mt-1">Zero clicks / pops</p>
                </div>
              </div>

              {/* Live Binary Packet Inspector */}
              <div className="bg-[#12151E] border border-[#202534] rounded-xl p-5">
                <div className="flex items-center justify-between mb-3">
                  <div className="flex items-center space-x-2">
                    <Terminal className="w-4 h-4 text-[#D4AF37]" />
                    <h3 className="text-sm font-semibold text-[#F0F3FA]">Live 28-Byte Binary Packet Header (Little-Endian)</h3>
                  </div>
                  <span className="text-[11px] font-mono text-[#8E98AC]">Port 50005</span>
                </div>
                <div className="bg-[#0A0C10] p-3 rounded-lg border border-[#1C202C] font-mono text-xs text-amber-300/90 overflow-x-auto tracking-wider">
                  {lastPacketHex}
                </div>
                <div className="grid grid-cols-2 sm:grid-cols-4 gap-2 mt-3 text-[11px] font-mono text-[#8E98AC]">
                  <div>Magic: <span className="text-emerald-400">'WFHF'</span></div>
                  <div>Ver: <span className="text-emerald-400">0x01</span></div>
                  <div>Type: <span className="text-emerald-400">0x01 (PCM)</span></div>
                  <div>Payload: <span className="text-emerald-400">1764 B (10ms)</span></div>
                </div>
              </div>
            </div>

            {/* Right Column: Hardware Audio Controls */}
            <div className="space-y-6">
              {/* Volume & Mute */}
              <div className="bg-[#12151E] border border-[#202534] rounded-xl p-5">
                <div className="flex items-center justify-between mb-4">
                  <h3 className="text-sm font-semibold text-[#D4AF37] uppercase tracking-wider">Receiver DAC Volume</h3>
                  <span className="text-sm font-bold font-mono text-[#F0F3FA]">{isMuted ? 'MUTED' : `${volume}%`}</span>
                </div>

                <div className="flex items-center space-x-4 mb-4">
                  <input
                    type="range"
                    min="0"
                    max="100"
                    value={volume}
                    onChange={(e) => setVolume(parseInt(e.target.value))}
                    className="flex-1 accent-[#D4AF37] cursor-pointer"
                  />
                  <button
                    onClick={() => setIsMuted(!isMuted)}
                    className={`p-2 rounded-lg border transition-colors ${
                      isMuted
                        ? 'bg-rose-950/60 border-rose-800 text-rose-400'
                        : 'bg-[#1C202C] border-[#2A3040] text-[#F0F3FA] hover:border-[#D4AF37]'
                    }`}
                  >
                    {isMuted ? <VolumeX className="w-5 h-5" /> : <Volume2 className="w-5 h-5" />}
                  </button>
                </div>
                <p className="text-[11px] text-[#8E98AC]">
                  Software logarithmic attenuation applied prior to I2S DMA transmission.
                </p>
              </div>

              {/* Latency Profile */}
              <div className="bg-[#12151E] border border-[#202534] rounded-xl p-5">
                <h3 className="text-sm font-semibold text-[#D4AF37] uppercase tracking-wider mb-3">
                  Jitter Buffer Profile
                </h3>
                <div className="grid grid-cols-3 gap-2 mb-3">
                  {(['LOW_LATENCY', 'BALANCED', 'STABLE'] as const).map((mode) => (
                    <button
                      key={mode}
                      onClick={() => setLatencyMode(mode)}
                      className={`py-2 px-2 rounded-lg text-xs font-semibold font-mono border transition-all ${
                        latencyMode === mode
                          ? 'bg-[#D4AF37] text-black border-[#D4AF37]'
                          : 'bg-[#181C26] text-[#8E98AC] border-[#252A3A] hover:text-[#F0F3FA]'
                      }`}
                    >
                      {mode === 'LOW_LATENCY' ? 'Low (35ms)' : mode === 'BALANCED' ? 'Balanced' : 'Stable (120ms)'}
                    </button>
                  ))}
                </div>
                <p className="text-[11px] text-[#8E98AC]">
                  {latencyMode === 'LOW_LATENCY'
                    ? 'Target: 35ms latency. Optimized for gaming and video audio sync.'
                    : latencyMode === 'BALANCED'
                    ? 'Target: 70ms latency. Recommended for general music playback on typical Wi-Fi.'
                    : 'Target: 120ms latency. High jitter immunity for congested Wi-Fi environments.'}
                </p>
              </div>

              {/* Network Simulation Stress */}
              <div className="bg-[#12151E] border border-[#202534] rounded-xl p-5">
                <div className="flex items-center justify-between mb-3">
                  <h3 className="text-sm font-semibold text-[#D4AF37] uppercase tracking-wider">
                    Simulate Wi-Fi Packet Loss
                  </h3>
                  <span className="text-xs font-mono text-[#F0F3FA]">{(simulatedLossRate * 100).toFixed(0)}%</span>
                </div>
                <input
                  type="range"
                  min="0"
                  max="0.25"
                  step="0.01"
                  value={simulatedLossRate}
                  onChange={(e) => setSimulatedLossRate(parseFloat(e.target.value))}
                  className="w-full accent-[#D4AF37] cursor-pointer mb-2"
                />
                <p className="text-[11px] text-[#8E98AC]">
                  Test receiver jitter buffer underrun handling and zero-insertion concealment under real packet drop.
                </p>
              </div>

              {/* Hardware Specs */}
              <div className="bg-[#12151E] border border-[#202534] rounded-xl p-5">
                <h3 className="text-sm font-semibold text-[#D4AF37] uppercase tracking-wider mb-3">Hardware Node</h3>
                <div className="space-y-2 text-xs">
                  <div className="flex justify-between">
                    <span className="text-[#8E98AC]">MCU:</span>
                    <span className="font-mono text-[#F0F3FA]">Espressif ESP32-C3 RISC-V @ 160MHz</span>
                  </div>
                  <div className="flex justify-between">
                    <span className="text-[#8E98AC]">Audio DAC:</span>
                    <span className="font-mono text-[#F0F3FA]">NXP UDA1334A Low-Power Stereo</span>
                  </div>
                  <div className="flex justify-between">
                    <span className="text-[#8E98AC]">I2S Bus:</span>
                    <span className="font-mono text-[#F0F3FA]">BCLK: 4, WS: 5, DOUT: 6</span>
                  </div>
                  <div className="flex justify-between">
                    <span className="text-[#8E98AC]">Ring Buffer:</span>
                    <span className="font-mono text-[#F0F3FA]">48 KB Static FreeRTOS Mutex</span>
                  </div>
                </div>
              </div>
            </div>
          </div>
        )}

        {/* Tab 2: Hardware Pinout & Wiring */}
        {activeTab === 'hardware' && (
          <div className="bg-[#12151E] border border-[#202534] rounded-xl p-6 space-y-6">
            <div>
              <div className="inline-flex items-center gap-2 bg-[#D4AF37]/10 text-[#D4AF37] px-2.5 py-1 rounded-full text-xs font-semibold mb-2">
                <Cpu className="w-3.5 h-3.5" />
                Target Board: ESP32-C3 SuperMini (RISC-V 160MHz)
              </div>
              <h2 className="text-lg font-bold text-[#F0F3FA] mb-1">ESP32-C3 SuperMini to NXP UDA1334A Wiring & Flashing Guide</h2>
              <p className="text-xs text-[#8E98AC]">
                The ESP32-C3 SuperMini (22.5 x 18 mm) provides direct access to GPIO 4, 5, 6, 5V, 3.3V, and GND on its standard headers.
                The NXP UDA1334A features an internal phase-locked loop (PLL) that automatically derives the master clock (MCLK) from WSEL. No dedicated MCLK pin is needed!
              </p>
            </div>

            <div className="overflow-x-auto">
              <table className="w-full text-left text-sm border-collapse">
                <thead>
                  <tr className="border-b border-[#202534] text-[#D4AF37] font-mono text-xs">
                    <th className="py-3 px-4">SuperMini Pin</th>
                    <th className="py-3 px-4">UDA1334A Pin</th>
                    <th className="py-3 px-4">Signal</th>
                    <th className="py-3 px-4">Description</th>
                  </tr>
                </thead>
                <tbody className="divide-y divide-[#1A1F2C] text-xs font-mono">
                  <tr>
                    <td className="py-3 px-4 font-bold text-amber-400">5V or 3V3</td>
                    <td className="py-3 px-4 font-bold text-amber-400">VIN</td>
                    <td className="py-3 px-4">Power Supply</td>
                    <td className="py-3 px-4 text-[#8E98AC]">Connect to SuperMini 5V (from USB) or 3.3V pin</td>
                  </tr>
                  <tr>
                    <td className="py-3 px-4 font-bold">GND</td>
                    <td className="py-3 px-4 font-bold">GND</td>
                    <td className="py-3 px-4">Ground</td>
                    <td className="py-3 px-4 text-[#8E98AC]">Common audio ground</td>
                  </tr>
                  <tr>
                    <td className="py-3 px-4 font-bold text-emerald-400">GPIO 4</td>
                    <td className="py-3 px-4 font-bold text-emerald-400">BCLK</td>
                    <td className="py-3 px-4">Bit Clock</td>
                    <td className="py-3 px-4 text-[#8E98AC]">Continuous I2S bit clock (1.4112 MHz)</td>
                  </tr>
                  <tr>
                    <td className="py-3 px-4 font-bold text-emerald-400">GPIO 5</td>
                    <td className="py-3 px-4 font-bold text-emerald-400">WSEL / LRCK</td>
                    <td className="py-3 px-4">Word Select</td>
                    <td className="py-3 px-4 text-[#8E98AC]">Left/Right channel clock (44.1 kHz)</td>
                  </tr>
                  <tr>
                    <td className="py-3 px-4 font-bold text-emerald-400">GPIO 6</td>
                    <td className="py-3 px-4 font-bold text-emerald-400">DIN / DATA</td>
                    <td className="py-3 px-4">Data Out</td>
                    <td className="py-3 px-4 text-[#8E98AC]">Serial PCM data input to DAC</td>
                  </tr>
                  <tr>
                    <td className="py-3 px-4 text-gray-500">(Leave open)</td>
                    <td className="py-3 px-4 text-gray-500">MCLK</td>
                    <td className="py-3 px-4 text-gray-500">Master Clock</td>
                    <td className="py-3 px-4 text-[#8E98AC]">Leave disconnected; UDA1334A self-generates MCLK via PLL</td>
                  </tr>
                </tbody>
              </table>
            </div>

            {/* Flashing instructions for SuperMini */}
            <div className="bg-[#0A0C10] p-4 rounded-lg border border-[#1A1F2C] space-y-3 text-xs">
              <h3 className="font-semibold text-[#D4AF37] font-mono flex items-center gap-2">
                <Zap className="w-4 h-4 text-amber-400" />
                Flashing Firmware to ESP32-C3 SuperMini (USB-C & OTA)
              </h3>
              <div className="space-y-2 text-[#8E98AC]">
                <p>
                  <strong className="text-white">1. Initial USB Flashing:</strong> Connect the SuperMini via USB-C. If your PC does not auto-detect the bootloader, hold the board's <code className="text-amber-400">BOOT</code> button (GPIO 9), press and release the <code className="text-amber-400">RESET</code> button, then release <code className="text-amber-400">BOOT</code>. Run:
                </p>
                <div className="bg-[#12151E] p-2.5 rounded font-mono text-emerald-400">
                  cd firmware && pio run -t upload
                </div>
                <p>
                  <strong className="text-white">Single-File Web Flasher (0x0 Offset):</strong> The build automatically produces <code className="text-amber-400">firmware-merged.bin</code> containing the bootloader, partitions table, boot_app0, and app binary. You can flash it directly using ESP Web Tools or WebSerial at base offset <code className="text-emerald-400">0x0</code>!
                </p>
                <p>
                  <strong className="text-white">2. Wireless OTA Push from Android App:</strong> Once initially flashed, you never need a USB cable again! You can open the WiFi-HiFi Android App &gt; Settings &gt; "Push Firmware to ESP32-C3 (OTA)" to flash newer <code className="text-white">firmware.bin</code> updates directly over Wi-Fi!
                </p>
              </div>
            </div>

            {/* Partition Configuration */}
            <div className="border-t border-[#202534] pt-6">
              <h3 className="text-sm font-semibold text-[#D4AF37] uppercase tracking-wider mb-2">
                4MB Flash Dual OTA Partition Layout (partitions.csv)
              </h3>
              <p className="text-xs text-[#8E98AC] mb-3">
                Configured with dual 1.5 MB application slots (<code className="text-amber-400">app0</code> and{' '}
                <code className="text-amber-400">app1</code>) allowing zero-downtime, failsafe OTA updates from the web interface.
              </p>
              <div className="bg-[#0A0C10] p-4 rounded-lg font-mono text-xs text-[#8E98AC] border border-[#1A1F2C]">
                nvs, data, nvs, 0x9000, 0x5000,<br />
                otadata, data, ota, 0xe000, 0x2000,<br />
                app0, app, ota_0, 0x10000, 0x180000, (1.5 MB)<br />
                app1, app, ota_1, 0x190000, 0x180000, (1.5 MB)<br />
                spiffs, data, spiffs, 0x310000, 0xe0000,<br />
                coredump, data, coredump, 0x3f0000, 0x10000
              </div>
            </div>
          </div>
        )}

        {/* Tab 3: Binary Protocol */}
        {activeTab === 'protocol' && (
          <div className="bg-[#12151E] border border-[#202534] rounded-xl p-6 space-y-6">
            <div>
              <h2 className="text-lg font-bold text-[#F0F3FA] mb-1">WiFi-HiFi Binary Protocol (WIFI_HIFI_PROTOCOL.md)</h2>
              <p className="text-xs text-[#8E98AC]">
                The streaming protocol utilizes fixed 28-byte binary headers prepended to 10ms PCM audio frames, transmitted
                over UDP to eliminate TCP retransmission stalls.
              </p>
            </div>

            <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
              <div className="bg-[#0A0C10] p-4 rounded-lg border border-[#1C202C] space-y-2 text-xs">
                <h3 className="font-semibold text-[#D4AF37] font-mono">Header Structure (28 Bytes, Little-Endian)</h3>
                <ul className="space-y-1 font-mono text-[#8E98AC]">
                  <li>• <span className="text-amber-400">0x00-0x03</span>: Magic <code className="text-white">0x46484657</code> ("WFHF")</li>
                  <li>• <span className="text-amber-400">0x04</span>: Version (<code className="text-white">0x01</code>)</li>
                  <li>• <span className="text-amber-400">0x05</span>: Packet Type (<code className="text-white">0x01 = PCM Audio</code>)</li>
                  <li>• <span className="text-amber-400">0x06-0x07</span>: Stream ID (16-bit random)</li>
                  <li>• <span className="text-amber-400">0x08-0x0B</span>: Sequence Number (32-bit monotonically increasing)</li>
                  <li>• <span className="text-amber-400">0x0C-0x13</span>: Cumulative Frame Position (64-bit sample counter)</li>
                  <li>• <span className="text-amber-400">0x14-0x17</span>: Sample Rate (<code className="text-white">44100 Hz</code>)</li>
                  <li>• <span className="text-amber-400">0x18</span>: Channels (<code className="text-white">2 = Stereo</code>)</li>
                  <li>• <span className="text-amber-400">0x19</span>: Bits Per Sample (<code className="text-white">16-bit</code>)</li>
                  <li>• <span className="text-amber-400">0x1A</span>: Latency Mode (<code className="text-white">0=Low, 1=Bal, 2=Stab</code>)</li>
                  <li>• <span className="text-amber-400">0x1B</span>: Reserved (<code className="text-white">0x00</code>)</li>
                  <li>• <span className="text-amber-400">0x1C-0x1D</span>: Payload Length (<code className="text-white">1764 Bytes</code>)</li>
                  <li>• <span className="text-amber-400">0x1E-0x1F</span>: Checksum (<code className="text-white">Optional CRC/Fletcher</code>)</li>
                </ul>
              </div>

              <div className="bg-[#0A0C10] p-4 rounded-lg border border-[#1C202C] space-y-3 text-xs">
                <h3 className="font-semibold text-[#D4AF37] font-mono">Service Discovery & Ports</h3>
                <div className="space-y-2 text-[#8E98AC]">
                  <div>
                    <span className="font-semibold text-white">Audio UDP Port:</span> <code className="text-amber-400">50005</code>
                    <p className="text-[11px]">Lossless binary PCM stream ingestion.</p>
                  </div>
                  <div>
                    <span className="font-semibold text-white">Discovery UDP Port:</span> <code className="text-amber-400">50006</code>
                    <p className="text-[11px]">Broadcast listener responding with <code className="text-emerald-400">WFHF_BEACON</code> JSON payload.</p>
                  </div>
                  <div>
                    <span className="font-semibold text-white">mDNS Service:</span> <code className="text-amber-400">_wifihifi._udp.local</code>
                    <p className="text-[11px]">Zero-configuration local network discovery.</p>
                  </div>
                  <div>
                    <span className="font-semibold text-white">HTTP REST API:</span> <code className="text-amber-400">Port 80</code>
                    <p className="text-[11px]">Real-time status, volume control, and dual-slot OTA firmware upload.</p>
                  </div>
                </div>
              </div>
            </div>
          </div>
        )}

        {/* Tab 4: Android Architecture */}
        {activeTab === 'android' && (
          <div className="bg-[#12151E] border border-[#202534] rounded-xl p-6 space-y-6">
            <div>
              <h2 className="text-lg font-bold text-[#F0F3FA] mb-1">Android 16+ Sender Architecture</h2>
              <p className="text-xs text-[#8E98AC]">
                Built using Android's modern <code className="text-amber-400">AudioPlaybackCapture</code> framework with
                a dedicated MediaProjection foreground service.
              </p>
            </div>

            <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
              <div className="bg-[#0A0C10] p-4 rounded-lg border border-[#1C202C] space-y-2 text-xs">
                <div className="w-8 h-8 rounded-lg bg-[#1F2430] flex items-center justify-center text-[#D4AF37] font-bold">1</div>
                <h3 className="font-semibold text-[#F0F3FA]">AudioPlaybackCapture</h3>
                <p className="text-[#8E98AC]">
                  Captures system audio output with matching usages <code className="text-amber-400">USAGE_MEDIA</code> and{' '}
                  <code className="text-amber-400">USAGE_GAME</code> without requiring root or audio hal patches.
                </p>
              </div>

              <div className="bg-[#0A0C10] p-4 rounded-lg border border-[#1C202C] space-y-2 text-xs">
                <div className="w-8 h-8 rounded-lg bg-[#1F2430] flex items-center justify-center text-[#D4AF37] font-bold">2</div>
                <h3 className="font-semibold text-[#F0F3FA]">Zero-Allocation Pipeline</h3>
                <p className="text-[#8E98AC]">
                  Pre-allocated byte buffers in <code className="text-amber-400">PcmPacketizer.kt</code> format exactly 10ms of
                  16-bit 44.1kHz audio per packet to ensure zero GC pauses in the audio thread.
                </p>
              </div>

              <div className="bg-[#0A0C10] p-4 rounded-lg border border-[#1C202C] space-y-2 text-xs">
                <div className="w-8 h-8 rounded-lg bg-[#1F2430] flex items-center justify-center text-[#D4AF37] font-bold">3</div>
                <h3 className="font-semibold text-[#F0F3FA]">Foreground Service</h3>
                <p className="text-[#8E98AC]">
                  <code className="text-amber-400">AudioStreamService</code> holds high-performance Wi-Fi and wake locks so
                  streaming continues uninterrupted with the screen turned off.
                </p>
              </div>
            </div>

            {/* ESP32-C3 SuperMini Push Capabilities */}
            <div className="bg-[#0A0C10] p-4 rounded-lg border border-[#1C202C] space-y-3 text-xs">
              <h3 className="font-semibold text-[#D4AF37] font-mono flex items-center gap-2">
                <Radio className="w-4 h-4 text-emerald-400" />
                Two Ways the Android App Pushes to ESP32-C3 SuperMini
              </h3>
              <div className="grid grid-cols-1 md:grid-cols-2 gap-3 text-[#8E98AC]">
                <div className="p-3 bg-[#12151E] rounded-lg border border-[#202534]">
                  <h4 className="font-bold text-white mb-1">1. Audio Push (Continuous Real-Time)</h4>
                  <p>
                    Captures 44.1kHz 16-bit stereo PCM from Android 16+ apps (Spotify, YouTube, games) and pushes 10ms packets
                    directly to ESP32-C3 SuperMini over Wi-Fi UDP port <code className="text-amber-400">50005</code> with sub-25ms latency.
                  </p>
                </div>
                <div className="p-3 bg-[#12151E] rounded-lg border border-[#202534]">
                  <h4 className="font-bold text-white mb-1">2. Firmware Push (Wireless OTA Update)</h4>
                  <p>
                    Tap Settings &gt; <em>"Select &amp; Push firmware.bin"</em> on your phone to upload new compiled binary firmware
                    directly into the ESP32-C3 SuperMini's dual OTA partition (<code className="text-amber-400">/api/ota</code>) without a PC!
                  </p>
                </div>
              </div>
            </div>

            <div className="bg-[#0A0C10] p-4 rounded-lg border border-[#1C202C] space-y-2 text-xs font-mono text-[#8E98AC]">
              <span className="text-[#D4AF37] font-bold">Quick Build Command:</span>
              <p className="text-white">cd android && ./gradlew assembleDebug</p>
              <p className="text-emerald-400">Output: app/build/outputs/apk/debug/app-debug.apk</p>
            </div>
          </div>
        )}
      </main>

      {/* Footer */}
      <footer className="border-t border-[#1C202C] bg-[#0A0C10] px-6 py-3 text-center text-xs text-[#8E98AC]">
        WiFi-HiFi Ecosystem • Repository: <span className="font-mono text-[#D4AF37]">anaya2025/WiFi-HiFi</span>
      </footer>
    </div>
  );
}
