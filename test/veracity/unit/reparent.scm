;;; Test for Metacity
;;; Do windows get reparented when the WM starts?
;;;
;;; Copyright (C) 2008 Thomas Thurman
;;; 
;;; This program is free software; you can redistribute it and/or
;;; modify it under the terms of the GNU General Public License as
;;; published by the Free Software Foundation; either version 2 of the
;;; License, or (at your option) any later version.
;;;
;;; This program is distributed in the hope that it will be useful, but
;;; WITHOUT ANY WARRANTY; without even the implied warranty of
;;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;;; General Public License for more details.
;;; 
;;; You should have received a copy of the GNU General Public License
;;; along with this program; if not, write to the Free Software
;;; Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
;;; 02111-1307, USA.

(define test
  (lambda ()
    (define a (make-window))
    (define b (make-window))
    (if (or (parent a) (parent b))
	"Window has a parent at the beginning"
	;;; otherwise let's start the wm
	(begin
	  (start-wm)
	  ;;; now, do we have a parent?
	  (if (or (not (parent a)) (not (parent b)))
	      "Window was not reparented"
	      ;;; otherwise pass
	      #t)
	  )
	)))
