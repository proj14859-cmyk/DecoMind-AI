"""Limite diario de peticiones (en memoria; se reinicia al reiniciar el servidor)."""
import datetime


class DailyQuota:
    def __init__(self, limit: int):
        self.limit = limit
        self._day = None
        self._used = 0

    def _roll(self):
        today = datetime.date.today().isoformat()
        if self._day != today:
            self._day, self._used = today, 0

    @property
    def used(self) -> int:
        self._roll()
        return self._used

    def take(self) -> bool:
        """Cuenta una peticion. False si ya se alcanzo el limite."""
        self._roll()
        if self._used >= self.limit:
            return False
        self._used += 1
        return True
