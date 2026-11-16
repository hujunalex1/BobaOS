import { Stage } from '../../kit'
import { Cloud, RotateCw, Sun } from 'lucide-react'
import './weather.css'

// A 240 × 240 weather app for the BobaOS watch (Waveshare ESP32-S3 1.54").
// The screen is drawn at its real 240px design size and scaled 2x inside the
// device shell, so type, radii and paddings are the shipped values.
export default function BobaWeather() {
  return (
    <Stage width={900} height={660} className="bw-stage">
      {() => (
        <div className="bw">
          <div className="bw-watch">
            <div className="bw-bezel" />
            <div className="bw-screen">
              <div className="bw-ui">
                <div className="bw-head">
                  <span className="bw-city">杭州</span>
                  <span className="bw-sync">
                    <RotateCw size={11} strokeWidth={2} />
                  </span>
                </div>

                <div className="bw-hero">
                  <div className="bw-temp">
                    26<span className="bw-deg">°</span>
                  </div>
                  <div className="bw-icon">
                    <Sun size={26} strokeWidth={1.6} className="bw-sun" />
                    <Cloud size={34} strokeWidth={1.5} className="bw-cloud" />
                  </div>
                </div>

                <div className="bw-cond">多云 · 体感 27°</div>

                <div className="bw-stats">
                  <div className="bw-stat">
                    <span className="bw-k">最高</span>
                    <span className="bw-v">28°</span>
                  </div>
                  <div className="bw-stat">
                    <span className="bw-k">最低</span>
                    <span className="bw-v">21°</span>
                  </div>
                  <div className="bw-stat">
                    <span className="bw-k">降雨</span>
                    <span className="bw-v">30%</span>
                  </div>
                </div>

                <div className="bw-next">明天 24° · 后天 26°</div>
              </div>
            </div>
          </div>
        </div>
      )}
    </Stage>
  )
}
