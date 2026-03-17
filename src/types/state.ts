export interface USState {
  code: string;
  name: string;
  region: string;
  is_active: boolean;
}

export interface StateStats {
  code: string;
  name: string;
  region: string;
  total_shelters: number;
  active_shelters: number;
  total_dogs: number;
  available_dogs: number;
  critical_dogs: number;
  high_urgency_dogs: number;
  avg_wait_days: number;
}
