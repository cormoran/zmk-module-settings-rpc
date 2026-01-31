/**
 * ZMK Core Settings - Main Application
 * Configure ZMK core settings like sleep/idle timeouts
 */

import "./App.css";
import { connect as serial_connect } from "@zmkfirmware/zmk-studio-ts-client/transport/serial";
import { ZMKConnection } from "@cormoran/zmk-studio-react-hook";
import { ActivitySettings } from "./ActivitySettings";

function App() {
  return (
    <div className="app">
      <header className="app-header">
        <h1>⚙️ ZMK Core Settings</h1>
        <p>Configure sleep, idle, and other ZMK settings</p>
      </header>

      <ZMKConnection
        renderDisconnected={({ connect, isLoading, error }) => (
          <section className="card">
            <h2>Device Connection</h2>
            {isLoading && <p>⏳ Connecting...</p>}
            {error && (
              <div className="error-message">
                <p>🚨 {error}</p>
              </div>
            )}
            {!isLoading && (
              <button
                className="btn btn-primary"
                onClick={() => connect(serial_connect)}
              >
                🔌 Connect Serial
              </button>
            )}
          </section>
        )}
        renderConnected={({ disconnect, deviceName }) => (
          <>
            <section className="card">
              <h2>Device Connection</h2>
              <div className="device-info">
                <h3>✅ Connected to: {deviceName}</h3>
              </div>
              <button className="btn btn-secondary" onClick={disconnect}>
                Disconnect
              </button>
            </section>

            <ActivitySettings />
          </>
        )}
      />

      <footer className="app-footer">
        <p>
          <strong>ZMK Core Settings Module</strong> - Configure your keyboard
          settings
        </p>
      </footer>
    </div>
  );
}

export default App;
