#!/bin/bash
#
# Coffee pot, uses HTCPCP protocol (RFC 2324)

# f() {
#   local b="$(cat dsfa)"
#   echo $?

# }

# f

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
  echo "Content-Type: text/html"
  echo "Safe: yes"
  echo

  echo BREWING
elif [[ "$REQUEST_METHOD" == "PROPFIND" ]]; then
  if [[ "${REQUEST_PATH}" =~ .*/([^./]+) ]]; then
    echo "Content-Type: text/plain"
    echo
    pot="${BASH_REMATCH[1]}"
    cat "pots/${pot}"
  else
    echo "Status: 418 I am a teapot"
    echo "Content-Type: text/plain"
    echo
  fi
fi

# show_form




# echo "<b>Hello</b>"
# echo "$RANDOM"

# read a
# sleep 10
# echo $a


# set | sed 's/$/<br>/'
# sleep 1h


#num='a[$(cat aaaa)]'
#echo "$((num+1))"