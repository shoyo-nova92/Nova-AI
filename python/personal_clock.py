"""IANA timezone observations, independent of the Windows host's local clock."""
import argparse
import datetime as dt
import json
from zoneinfo import ZoneInfo, ZoneInfoNotFoundError


def observe(timezone, now=None):
    now = now or dt.datetime.now(dt.timezone.utc)
    if now.tzinfo is None:
        raise ValueError('Clock instant must have a timezone')
    local = now.astimezone(ZoneInfo(timezone))
    return {'success': True, 'timezone': timezone, 'local_date': local.date().isoformat(),
            'weekday': local.strftime('%A'), 'weekday_iso': local.isoweekday(),
            'local_datetime': local.isoformat(), 'utc_datetime': now.astimezone(dt.timezone.utc).isoformat(),
            'utc_offset_seconds': int(local.utcoffset().total_seconds())}


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--timezone', required=True)
    args = parser.parse_args()
    try:
        print(json.dumps(observe(args.timezone)))
    except (ValueError, ZoneInfoNotFoundError) as error:
        print(json.dumps({'success': False, 'category': 'configuration',
                          'message': f'Cannot resolve configured IANA timezone: {error}. Install tzdata in Nova\'s Python environment; do not guess UTC.'}))
