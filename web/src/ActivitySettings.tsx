/**
 * Activity Settings Component
 * Allows getting and setting sleep/idle timeout settings
 */

import { useContext, useState, useEffect } from "react";
import {
  ZMKCustomSubsystem,
  ZMKAppContext,
} from "@cormoran/zmk-studio-react-hook";
import { Request, Response } from "./proto/zmk/settings/core";

// Custom subsystem identifier - must match firmware registration
export const SUBSYSTEM_IDENTIFIER = "zmk__settings";

export interface ActivitySettingsProps {
  /**
   * Whether to automatically fetch settings on mount.
   * Defaults to true. Set to false in tests to avoid automatic RPC calls.
   */
  autoFetch?: boolean;
}

export function ActivitySettings({ autoFetch = true }: ActivitySettingsProps) {
  const zmkApp = useContext(ZMKAppContext);
  const [idleMs, setIdleMs] = useState<number>(30000); // 30 seconds default
  const [sleepMs, setSleepMs] = useState<number>(900000); // 15 minutes default
  const [isLoading, setIsLoading] = useState(false);
  const [message, setMessage] = useState<string | null>(null);
  const [error, setError] = useState<string | null>(null);

  if (!zmkApp) return null;

  const subsystem = zmkApp.findSubsystem(SUBSYSTEM_IDENTIFIER);

  // Get current settings when component mounts or subsystem becomes available
  useEffect(() => {
    if (subsystem && zmkApp.state.connection && autoFetch) {
      getCurrentSettings();
    }
  }, [subsystem, zmkApp.state.connection, autoFetch]);

  const getCurrentSettings = async () => {
    if (!zmkApp.state.connection || !subsystem) return;

    setIsLoading(true);
    setError(null);
    setMessage(null);

    try {
      const service = new ZMKCustomSubsystem(
        zmkApp.state.connection,
        subsystem.index
      );

      const request = Request.create({
        getActivitySettings: {},
      });

      const payload = Request.encode(request).finish();
      const responsePayload = await service.callRPC(payload);

      if (responsePayload) {
        const resp = Response.decode(responsePayload);

        if (resp.getActivitySettings) {
          const settings = resp.getActivitySettings.settings;
          if (settings) {
            setIdleMs(settings.idleMs);
            setSleepMs(settings.sleepMs);
            setMessage("Settings loaded successfully");
          }
        } else if (resp.error) {
          setError(`Error: ${resp.error.message}`);
        }
      }
    } catch (err) {
      console.error("Failed to get activity settings:", err);
      setError(
        `Failed: ${err instanceof Error ? err.message : "Unknown error"}`
      );
    } finally {
      setIsLoading(false);
    }
  };

  const updateSettings = async () => {
    if (!zmkApp.state.connection || !subsystem) return;

    setIsLoading(true);
    setError(null);
    setMessage(null);

    try {
      const service = new ZMKCustomSubsystem(
        zmkApp.state.connection,
        subsystem.index
      );

      const request = Request.create({
        setActivitySettings: {
          settings: {
            idleMs: idleMs,
            sleepMs: sleepMs,
          },
        },
      });

      const payload = Request.encode(request).finish();
      const responsePayload = await service.callRPC(payload);

      if (responsePayload) {
        const resp = Response.decode(responsePayload);

        if (resp.setActivitySettings) {
          if (resp.setActivitySettings.success) {
            setMessage("Settings updated successfully!");
          } else {
            setError("Failed to update settings");
          }
        } else if (resp.error) {
          setError(`Error: ${resp.error.message}`);
        }
      }
    } catch (err) {
      console.error("Failed to set activity settings:", err);
      setError(
        `Failed: ${err instanceof Error ? err.message : "Unknown error"}`
      );
    } finally {
      setIsLoading(false);
    }
  };

  if (!subsystem) {
    return (
      <section className="card">
        <div className="warning-message">
          <p>
            ⚠️ Subsystem "{SUBSYSTEM_IDENTIFIER}" not found. Make sure your
            firmware includes the settings module.
          </p>
        </div>
      </section>
    );
  }

  return (
    <section className="card">
      <h2>⏱️ Activity Settings (Sleep/Idle Timeout)</h2>
      <p>Configure how long the keyboard waits before going idle or to sleep.</p>

      <div className="settings-grid">
        <div className="input-group">
          <label htmlFor="idle-timeout">Idle Timeout (ms):</label>
          <input
            id="idle-timeout"
            type="number"
            value={idleMs}
            onChange={(e) => setIdleMs(parseInt(e.target.value) || 0)}
            disabled={isLoading}
            min="0"
            step="1000"
          />
          <small>
            {idleMs === 0
              ? "Disabled"
              : `${(idleMs / 1000).toFixed(0)} seconds`}
          </small>
        </div>

        <div className="input-group">
          <label htmlFor="sleep-timeout">Sleep Timeout (ms):</label>
          <input
            id="sleep-timeout"
            type="number"
            value={sleepMs}
            onChange={(e) => setSleepMs(parseInt(e.target.value) || 0)}
            disabled={isLoading}
            min="0"
            step="1000"
          />
          <small>
            {sleepMs === 0
              ? "Disabled"
              : `${(sleepMs / 60000).toFixed(1)} minutes`}
          </small>
        </div>
      </div>

      <div className="button-group">
        <button
          className="btn btn-secondary"
          disabled={isLoading}
          onClick={getCurrentSettings}
        >
          {isLoading ? "⏳ Loading..." : "🔄 Refresh"}
        </button>
        <button
          className="btn btn-primary"
          disabled={isLoading}
          onClick={updateSettings}
        >
          {isLoading ? "⏳ Updating..." : "💾 Save Settings"}
        </button>
      </div>

      {message && (
        <div className="success-message">
          <p>✅ {message}</p>
        </div>
      )}

      {error && (
        <div className="error-message">
          <p>🚨 {error}</p>
        </div>
      )}
    </section>
  );
}
