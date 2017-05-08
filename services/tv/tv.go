package main

import (
	"crypto/hmac"
	"crypto/sha256"
	"encoding/gob"
	"encoding/hex"
	"fmt"
	"hash"
	"html/template"
	"io/ioutil"
	"log"
	"math/rand"
	"mime/multipart"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"sort"
	"strings"
	"time"
)

type Input struct {
	File multipart.File
	Name string
	Info string
	User string
}

const MaxBodySize = 450 * 1024
const Secret = "b0d83b8f905949687141083089d1119f"

var queue chan Input
var r *rand.Rand
var users map[string]string
var sig hash.Hash

func main() {
	queue = make(chan Input, 64)
	go input_processor()

	users = make(map[string]string)
	f, err := os.OpenFile("users.gob", os.O_RDWR|os.O_CREATE, 0644)
	if err != nil {
		log.Fatal(err)
	}
	decoder := gob.NewDecoder(f)
	decoder.Decode(&users)
	if err := f.Close(); err != nil {
		log.Fatal(err)
	}

	sig = hmac.New(sha256.New, []byte (Secret))

	r = rand.New(rand.NewSource(time.Now().UnixNano()))

	http.Handle("/static/", http.StripPrefix("/static/", http.FileServer(http.Dir("./static"))))

	http.HandleFunc("/create", create_handler)
	http.HandleFunc("/logout", logout_handler)
	http.HandleFunc("/login", login_handler)
	http.HandleFunc("/upload", upload_handler)
	http.HandleFunc("/",       index_handler)

	server := &http.Server{
		Addr:         ":8080",
		ReadTimeout:  30 * time.Second,
		WriteTimeout: 30 * time.Second,
	}

	if err := server.ListenAndServe(); err != nil {
		log.Fatal(err)
	}
}

func index_handler(w http.ResponseWriter, r *http.Request) {
	f, _ := r.Cookie("flash")
	http.SetCookie(w, &http.Cookie{Name: "flash", Value: "", MaxAge: -1})

	c, _ := r.Cookie("user")
	t, _ := template.ParseFiles("templates/index.tmpl")

	var images []string
	if c != nil {
		s, _ := r.Cookie("sign")
		if s == nil {
			http.SetCookie(w, &http.Cookie{Name: "flash", Value: "Invalid signature"})
			http.SetCookie(w, &http.Cookie{Name: "user", Value: "", MaxAge: -1})
			http.Redirect(w, r, "/", 302)
			return
		}
		sign := sign(c.Value)
		if s.Value != sign {
			http.SetCookie(w, &http.Cookie{Name: "flash", Value: "Invalid signature"})
			http.SetCookie(w, &http.Cookie{Name: "user", Value: "", MaxAge: -1})
			http.Redirect(w, r, "/", 302)
			return
		}

		files, _ := ioutil.ReadDir(filepath.Join("static", c.Value))
		size := 30
		if len(files) < size {
			size = len(files)
		}
		images = make([]string, size)
		for i, v := range files[:size] {
			images[i] = v.Name()
		}
		sort.Sort(sort.Reverse(sort.StringSlice(images)))
	}

	var x = struct {
		Flash  *http.Cookie
		User   *http.Cookie
		Images []string
	}{f, c, images}
	if err := t.Execute(w, &x); err != nil {
		log.Fatal(err)
	}
}

func create_handler(w http.ResponseWriter, r *http.Request) {
	user := random_string(12)
	pass := random_string(8)
	users[user] = pass

	cookie := &http.Cookie{Name: "user", Value: user}
	http.SetCookie(w, cookie)

	sign_user := sign(user)
	sign_cookie := &http.Cookie{Name: "sign", Value: sign_user}
	http.SetCookie(w, sign_cookie)

	flash := fmt.Sprintf("Your password is '%s'", pass)
	http.SetCookie(w, &http.Cookie{Name: "flash", Value: flash})
	http.Redirect(w, r, "/", 302)

	go func () {
		f, err := os.OpenFile("users.gob", os.O_RDWR, 0644)
		if err != nil {
			log.Fatal(err)
		}
		encoder := gob.NewEncoder(f)
		encoder.Encode(users)
		f.Close()
	}()
}

func login_handler(w http.ResponseWriter, r *http.Request) {
	login := r.FormValue("login")
	pass  := r.FormValue("pass")

	var flash = ""
	if p, ok := users[login]; ok == true {
		if (pass == p) {
			http.SetCookie(w, &http.Cookie{Name: "user", Value: login})
			sign_user := sign(login)
			sign_cookie := &http.Cookie{Name: "sign", Value: sign_user}
			http.SetCookie(w, sign_cookie)
		} else {
			flash = "Invalid password"
		}
	} else {
		flash = "No such user"
	}

	http.SetCookie(w, &http.Cookie{Name: "flash", Value: flash})
	http.Redirect(w, r, "/", 302)
}

func logout_handler(w http.ResponseWriter, r *http.Request) {
	http.SetCookie(w, &http.Cookie{Name: "user", Value: "", MaxAge: -1})
	http.Redirect(w, r, "/", 302)
}

func upload_handler(w http.ResponseWriter, r *http.Request) {
	r.Body = http.MaxBytesReader(w, r.Body, MaxBodySize)

	c, _ := r.Cookie("user")
	if c == nil {
		http.SetCookie(w, &http.Cookie{Name: "flash", Value: "Login first"})
		http.Redirect(w, r, "/", 302)
		return
	}

	s, _ := r.Cookie("sign")
	if s == nil {
		http.SetCookie(w, &http.Cookie{Name: "flash", Value: "Invalid signature"})
		http.SetCookie(w, &http.Cookie{Name: "user", Value: "", MaxAge: -1})
		http.Redirect(w, r, "/", 302)
		return
	}
	sign := sign(c.Value)
	if s.Value != sign {
		http.SetCookie(w, &http.Cookie{Name: "flash", Value: "Invalid signature"})
		http.SetCookie(w, &http.Cookie{Name: "user", Value: "", MaxAge: -1})
		http.Redirect(w, r, "/", 302)
		return
	}

	flash := "Your photo will be processed soon"

	if r.Method == "POST" {
		f, h, err := r.FormFile("image")
		ct := h.Header["Content-Type"]
		if (ct[0] == "image/png" || ct[0] == "image/jpeg") == false {
			flash = "Invalid image format"
		} else if (strings.Contains(h.Filename, ".png") || strings.Contains(h.Filename, ".jpg")) == false {
			flash = "Invalid image file"
		} else {
			if err != nil {
				flash = err.Error()
			} else {
				queue <- Input{f, h.Filename, r.FormValue("info"), c.Value}
			}
		}
	} else {
		flash = "Invalid http method"
	}

	http.SetCookie(w, &http.Cookie{Name: "flash", Value: flash})
	http.Redirect(w, r, "/", 302)
}

func input_processor() {
	for i := range queue {
		b, _ := ioutil.ReadAll(i.File)

		origin := filepath.Join("origin", i.Name)
		name := fmt.Sprintf("%d-%s.png", time.Now().Unix(), i.Name)
		dst := filepath.Join("static", i.User, name);
		_ = os.MkdirAll(filepath.Join("static", i.User), 0755)

		ioutil.WriteFile(origin, b, 0644)
		cmd := exec.Command(
			"convert", origin,
			"-resize", "128x128",
			"-set", "comment", i.Info,
			dst)
		cmd.Env = []string{"MAGICK_CONFIGURE_PATH=/opt/tv/im/"}

		cmd.Run()
	}
}

func random_string(n int) string {
	const chars = "abcdefghijklmnopqrstuvwxyz0123456789"
	result := make([]byte, n)
	for i := range result {
		result[i] = chars[r.Intn(len(chars))]
	}
	return string(result)
}

func sign(s string) string {
	sig.Reset()
	sig.Write([]byte (s))

	signature := sig.Sum(nil)
	return hex.EncodeToString(signature)
}
