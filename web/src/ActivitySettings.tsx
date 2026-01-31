/**
 * Activity Settings Component
 * Allows getting and setting sleep/idle timeout settings
 */

import { useContext, useState, useEffect, useMemo, useCallback } from "react";
import {
  ZMKCustomSubsystem,
  ZMKAppContext,
} from "@cormoran/zmk-studio-react-hook";
import { Request, Response, Notification } from "./proto/zmk/settings/core";

// Custom subsystem identifier - must match firmware registration
export const SUBSYSTEM_IDENTIFIER = "zmk__settings";

interface DeviceSettings {
  source: number;
  idleMs: number;
  sleepMs: number;
}

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
  const [allDeviceSettings, setAllDeviceSettings] = useState<DeviceSettings[]>(
    []
  );
  const [showSyncWarning, setShowSyncWarning] = useState(false);

  // Memoize subsystem to prevent re-rendering on every render
  const subsystem = useMemo(
    () => zmkApp?.findSubsystem(SUBSYSTEM_IDENTIFIER),
    [zmkApp]
  );

  // Handle notifications from firmware
  const handleNotification = useCallback((notificationPayload: Uint8Array) => {
    try {
      const notification = Notification.decode(notificationPayload);
      if (notification.activitySettings?.settings) {
        const settings = notification.activitySettings.settings;
        const deviceSetting: DeviceSettings = {
          source: settings.source,
          idleMs: settings.idleMs,
          sleepMs: settings.sleepMs,
        };

        setAllDeviceSettings((prev) => {
          // Check if we already have settings from this source
          const filtered = prev.filter((s) => s.source !== settings.source);
          const updated = [...filtered, deviceSetting];

          // Update central's displayed values with central's settings (source 0)
          if (settings.source === 0) {
            setIdleMs(settings.idleMs);
            setSleepMs(settings.sleepMs);
          }

          // Check if all devices are in sync
          const inSync = updated.every(
            (s) =>
              s.idleMs === updated[0].idleMs && s.sleepMs === updated[0].sleepMs
          );
          setShowSyncWarning(!inSync && updated.length > 1);

          return updated;
        });
      }
    } catch (err) {
      console.error("Failed to decode notification:", err);
    }
  }, []);

  // Subscribe to notifications
  useEffect(() => {
    if (!zmkApp?.state.connection || !subsystem) return;

    // Register notification handler
    const unsubscribe =
      zmkApp.state.connection.subscribeToCustomNotifications?.(
        subsystem.index,
        handleNotification
      );

    return () => {
      unsubscribe?.();
    };
  }, [zmkApp?.state.connection, subsystem, handleNotification]);

  // Get current settings when component mounts or subsystem becomes available
  useEffect(() => {
    if (subsystem && zmkApp?.state.connection && autoFetch) {
      getCurrentSettings();
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [subsystem, zmkApp?.state.connection, autoFetch]);

  if (!zmkApp) return null;

  const getCurrentSettings = async () => {
    if (!zmkApp.state.connection || !subsystem) return;

    setIsLoading(true);
    setError(null);
    setMessage(null);
    setAllDeviceSettings([]); // Clear previous device settings
    setShowSyncWarning(false);

    try {
      const service = new ZMKCustomSubsystem(
        zmkApp.state.connection,
        subsystem.index
      );

      // Request settings from all devices (central + peripherals)
      // This triggers notifications from each device
      const request = Request.create({
        getAllActivitySettings: {},
      });

      const payload = Request.encode(request).finish();
      const responsePayload = await service.callRPC(payload);

      if (responsePayload) {
        const resp = Response.decode(responsePayload);

        if (resp.getAllActivitySettings) {
          if (resp.getAllActivitySettings.requestSent) {
            setMessage("Collecting settings from all devices...");
            // Actual settings will arrive via notifications
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

  const syncAllDevices = async () => {
    if (!zmkApp.state.connection || !subsystem) return;

    setIsLoading(true);
    setError(null);
    setMessage(null);

    try {
      const service = new ZMKCustomSubsystem(
        zmkApp.state.connection,
        subsystem.index
      );

      // Apply central's current settings to all devices
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
            setMessage("Settings synchronized across all devices!");
            setShowSyncWarning(false);
            // Refresh to verify sync
            setTimeout(() => getCurrentSettings(), 500);
          } else {
            setError("Failed to sync settings");
          }
        } else if (resp.error) {
          setError(`Error: ${resp.error.message}`);
        }
      }
    } catch (err) {
      console.error("Failed to sync settings:", err);
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
      <p>
        Configure how long the keyboard waits before going idle or to sleep.
      </p>

      {showSyncWarning && (
        <div className="warning-message">
          <p>
            ⚠️ <strong>Settings mismatch detected!</strong> Not all devices have
            the same settings.
          </p>
          <button
            className="btn btn-primary"
            onClick={syncAllDevices}
            disabled={isLoading}
          >
            🔄 Sync All Devices
          </button>
        </div>
      )}

      {allDeviceSettings.length > 1 && (
        <div className="device-settings-list">
          <h3>Device Settings:</h3>
          <ul>
            {allDeviceSettings.map((device) => (
              <li key={device.source}>
                <strong>
                  {device.source === 0
                    ? "Central"
                    : `Peripheral ${device.source}`}
                </strong>
                : Idle {device.idleMs}ms, Sleep {device.sleepMs}ms
              </li>
            ))}
          </ul>
        </div>
      )}

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
