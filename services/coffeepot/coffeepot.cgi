#!/bin/bash
#
# Coffee pot, uses HTCPCP protocol (RFC 2324)

show_form() {
  cat main_page.html
}

show_favicon() {
  cat favicon.ico
}

get_path() {
  if [[ "$REQUEST_URI" =~ ^([^?]*) ]]; then
    echo "${BASH_REMATCH[1]}"
  else
    echo "${REQUEST_URI}"
  fi
}

i_am_teapot() {
  echo "Status: 418 I am a teapot"
  echo "Content-Type: text/plain"
  echo
}

calc_temperature() {
  local secs="$1"
  local temp="$((20+secs/5))"

  if [[ temp -gt 100 ]]; then 
    echo 100
  else
    echo "${temp}"
  fi
}

REQUEST_PATH="$(get_path)"

if [[ "${REQUEST_METHOD}" == "GET" ]]; then
  if [[ "${REQUEST_PATH}" == "/" ]]; then
    echo "Content-Type: text/html"
    echo "Safe: yes"
    echo
    show_form
  elif [[ "$REQUEST_PATH" == "/favicon.ico" ]]; then
    echo "Content-Type: image/x-icon"
    echo
    show_favicon
  fi
elif [[ "$REQUEST_METHOD" == "BREW" || "$REQUEST_METHOD" == "POST" ]]; then
  if [[ "${REQUEST_PATH}" =~ .*/([^./]+) ]]; then
    echo "Content-Type: text/plain"
    echo
    pot="${BASH_REMATCH[1]}"
    if [ -f "pots/${pot}" ]; then
      echo EXISTS
    else
      echo "${HTTP_ACCEPT_ADDITIONS}" | tr ';,' ':\n' > "pots/${pot}"
      echo BREWING
    fi
  else
    i_am_teapot
  fi
elif [[ "$REQUEST_METHOD" == "PUT" ]]; then
  if [[ "${REQUEST_PATH}" =~ .*/([^./]+) ]]; then
    echo "Content-Type: text/plain"
    echo
    pot="${BASH_REMATCH[1]}"
    if [ ! -f "pots/${pot}" ]; then
      echo NO EXISTS
    else
      > "pots/${pot}.new"
      cat "pots/${pot}" | while read line; do
        addition="$(echo "${line}"|cut -d ':' -f 1)"
        count="$(echo "${line}"|cut -d ':' -f 2)"
        
        if [[ "${addition}" != "${HTTP_ADDITION}" ]]; then
          echo "${addition}:${count}" >> "pots/${pot}.new"
        else
          echo "${addition}:$((count+1))" >> "pots/${pot}.new"
        fi
      done

      # restore modification time
      touch -r "pots/${pot}" "pots/${pot}.new"
      mv "pots/${pot}.new" "pots/${pot}"
      echo ADDED
    fi
  else
    i_am_teapot
  fi
elif [[ "$REQUEST_METHOD" == "PROPFIND" ]]; then
  if [[ "${REQUEST_PATH}" =~ .*/([^./]+) ]]; then
    echo "Content-Type: text/plain"
    echo
    pot="${BASH_REMATCH[1]}"
    if [ -f "pots/${pot}" ]; then
      cat "pots/${pot}"
      # calculate temperature, just emulating
      time_now="$(date +%s)"
      time_file="$(stat --format="%Y" "pots/${pot}")"
      time_diff="$(( time_now - time_file ))"
      temperature="$(calc_temperature "${time_diff}")"
      echo "Temperature: ${temperature}"
      if [[ temperature -ge 95 ]]; then
        echo "State: READY"
      else
        echo "State: BREWING"
      fi
    fi
  else
    i_am_teapot
  fi
fi
