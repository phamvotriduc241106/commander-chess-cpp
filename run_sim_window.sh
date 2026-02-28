#!/usr/bin/env bash
set -euo pipefail

DURATION_SECS="${1:-7200}"
OUT_PREFIX="${2:-sim2h}"
BIN="${3:-./commanderchess_sim}"

start_utc="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
start_epoch="$(date -u +%s)"
end_target_epoch=$((start_epoch + DURATION_SECS))
end_target_utc="$(date -u -r "$end_target_epoch" +%Y-%m-%dT%H:%M:%SZ)"

run_ts="$(date -u +%Y%m%dT%H%M%SZ)"
out_dir="${OUT_PREFIX}_${run_ts}"
mkdir -p "$out_dir"

meta_file="$out_dir/meta.txt"
run_log="$out_dir/run.log"
raw_log="$out_dir/raw.log"
summary_file="$out_dir/summary.txt"

# metadata header
{
  echo "start_utc=$start_utc"
  echo "host=$(hostname)"
  echo "binary=$(cd "$(dirname "$BIN")" && pwd)/$(basename "$BIN")"
  echo "end_target_utc=$end_target_utc"
  echo "summary_file=$(pwd)/$summary_file"
  echo "raw_log=$(pwd)/$raw_log"
  echo "run_log=$(pwd)/$run_log"
} > "$meta_file"

runs=0
total_games=0
red_wins=0
blue_wins=0
draws=0
sum_engine_seconds=0

seed=100001

while :; do
  now_epoch="$(date -u +%s)"
  if (( now_epoch >= end_target_epoch )); then
    break
  fi

  runs=$((runs + 1))
  mod=$((runs % 3))

  if (( mod == 1 )); then
    profile="fast"
    games=12
    depth=2
    time_ms=20
    mcts_flag=""
    mcts_label="0"
  elif (( mod == 2 )); then
    profile="medium"
    games=8
    depth=4
    time_ms=50
    mcts_flag=""
    mcts_label="0"
  else
    profile="hard"
    games=3
    depth=6
    time_ms=80
    mcts_flag="--mcts"
    mcts_label="--mcts"
  fi

  run_utc="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "=== RUN $runs profile=$profile seed=$seed games=$games depth=$depth time_ms=$time_ms mcts=$mcts_label utc=$run_utc ===" | tee -a "$run_log"

  cmd=("$BIN" --sim --games "$games" --seed "$seed" --depth "$depth" --time_ms "$time_ms" --max_plies 300 --start alternate --eval_backend cpu)
  if [[ -n "$mcts_flag" ]]; then
    cmd+=("$mcts_flag")
  fi

  tmp_out="$out_dir/.run_${runs}.tmp"
  "${cmd[@]}" > "$tmp_out" 2>&1 || true

  {
    echo "--- RUN $runs OUTPUT BEGIN ---"
    cat "$tmp_out"
    echo "--- RUN $runs OUTPUT END ---"
  } >> "$raw_log"

  rw="$(grep -E '^RESULTS:' "$tmp_out" | sed -E 's/.*red_wins=([0-9]+).*/\1/' || echo 0)"
  bw="$(grep -E '^RESULTS:' "$tmp_out" | sed -E 's/.*blue_wins=([0-9]+).*/\1/' || echo 0)"
  dr="$(grep -E '^RESULTS:' "$tmp_out" | sed -E 's/.*draws=([0-9]+).*/\1/' || echo 0)"
  sec="$(grep -E '^total seconds:' "$tmp_out" | awk '{print $3}' || echo 0)"

  # Guard against parse failures
  [[ "$rw" =~ ^[0-9]+$ ]] || rw=0
  [[ "$bw" =~ ^[0-9]+$ ]] || bw=0
  [[ "$dr" =~ ^[0-9]+$ ]] || dr=0
  [[ "$sec" =~ ^[0-9]+(\.[0-9]+)?$ ]] || sec=0

  total_games=$((total_games + games))
  red_wins=$((red_wins + rw))
  blue_wins=$((blue_wins + bw))
  draws=$((draws + dr))
  sum_engine_seconds="$(awk -v a="$sum_engine_seconds" -v b="$sec" 'BEGIN{printf "%.3f", a+b}')"

  rm -f "$tmp_out"
  seed=$((seed + 1))

done

end_utc="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
{
  echo "end_utc=$end_utc"
} >> "$meta_file"

if (( total_games > 0 )); then
  red_rate="$(awk -v w="$red_wins" -v g="$total_games" 'BEGIN{printf "%.6f", w/g}')"
  blue_rate="$(awk -v w="$blue_wins" -v g="$total_games" 'BEGIN{printf "%.6f", w/g}')"
  draw_rate="$(awk -v d="$draws" -v g="$total_games" 'BEGIN{printf "%.6f", d/g}')"
else
  red_rate="0.000000"
  blue_rate="0.000000"
  draw_rate="0.000000"
fi

if awk -v s="$sum_engine_seconds" 'BEGIN{exit !(s>0)}'; then
  gph="$(awk -v g="$total_games" -v s="$sum_engine_seconds" 'BEGIN{printf "%.3f", (g*3600.0)/s}')"
else
  gph="0.000"
fi

{
  echo "runs=$runs"
  echo "total_games=$total_games"
  echo "red_wins=$red_wins"
  echo "blue_wins=$blue_wins"
  echo "draws=$draws"
  echo "red_win_rate=$red_rate"
  echo "blue_win_rate=$blue_rate"
  echo "draw_rate=$draw_rate"
  echo "sum_engine_seconds=$sum_engine_seconds"
  echo "games_per_hour_observed=$gph"
} > "$summary_file"

echo "done_out_dir=$out_dir"
