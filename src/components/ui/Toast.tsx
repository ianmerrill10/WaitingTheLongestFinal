"use client";

import { useState, useEffect, useCallback, createContext, useContext } from "react";

interface Toast {
  id: string;
  message: string;
  type: "success" | "error" | "info";
}

interface ToastContextType {
  toast: (message: string, type?: "success" | "error" | "info") => void;
}

const ToastContext = createContext<ToastContextType>({ toast: () => {} });

export function useToast() {
  return useContext(ToastContext);
}

export function ToastProvider({ children }: { children: React.ReactNode }) {
  const [toasts, setToasts] = useState<Toast[]>([]);

  const addToast = useCallback(
    (message: string, type: "success" | "error" | "info" = "info") => {
      const id = Math.random().toString(36).slice(2);
      setToasts((prev) => [...prev, { id, message, type }]);
    },
    []
  );

  const removeToast = useCallback((id: string) => {
    setToasts((prev) => prev.filter((t) => t.id !== id));
  }, []);

  return (
    <ToastContext.Provider value={{ toast: addToast }}>
      {children}
      <div className="fixed bottom-4 right-4 z-[9999] flex flex-col gap-2 max-w-sm" role="status" aria-live="polite">
        {toasts.map((t) => (
          <ToastItem key={t.id} toast={t} onDismiss={removeToast} />
        ))}
      </div>
    </ToastContext.Provider>
  );
}

function ToastItem({
  toast,
  onDismiss,
}: {
  toast: Toast;
  onDismiss: (id: string) => void;
}) {
  useEffect(() => {
    const timer = setTimeout(() => onDismiss(toast.id), 4000);
    return () => clearTimeout(timer);
  }, [toast.id, onDismiss]);

  const bgClass =
    toast.type === "success"
      ? "bg-green-700 text-white"
      : toast.type === "error"
        ? "bg-red-600 text-white"
        : "bg-gray-800 text-white";

  return (
    <div
      className={`${bgClass} px-4 py-3 rounded-lg shadow-lg text-sm animate-slide-up flex items-center justify-between gap-2`}
    >
      <span>{toast.message}</span>
      <button
        onClick={() => onDismiss(toast.id)}
        className="text-white/70 hover:text-white text-lg leading-none"
      >
        &times;
      </button>
    </div>
  );
}
