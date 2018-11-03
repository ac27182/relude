open Belt.Result;

/*
 Aff is an pure, lazy, asynchronous effect monad that should be able to handle any type of effect
 a program could possibly need, including effects like Option, Result, Js.Promise, Future, etc.  It basically boils down
 to a continuation callback, where the `onDone` callback is invoked with a `Result.t('a, 'e)`
 to either provide a value or an error.  The error type is left open (bifunctor IO), but for many JS FFI functions,
 the error type will be Js.Exn.t.

 This is inspired by bs-effects `Affect` and John De Goes' basic Async IO monad described here: http://degoes.net/articles/only-one-io

 Aff should be similar in spirit to the `IO` type Haskell, or the new row-less Effect type of purescript.

 See the unit tests (Aff_test.re) for an example of how this works with asynchronous functions that can fail.

 TODO:
 I'm not sure whether we need to return Eff.t(unit) in these methods, but that's what John De Goes' basic IO type is doing.  I think
 the idea is that the callback is itself an effect, so it should be treated as an effect rather than just something that returns `unit`.

 However, making the callback return Eff and the onDone function return Eff means we have to add extra layers of `() => ...` and `onDone(...)()`
 everywhere.  I'm not sure if that's useful or not.
 */

type t('a, 'e) = (Belt.Result.t('a, 'e) => Eff.t(unit)) => Eff.t(unit);

let run: t(unit, 'e) => unit = onDone => onDone(_ => Eff.pure(), ());

let pure: 'a => t('a, 'e) = (a, onDone) => onDone(Ok(a));

let map: ('a => 'b, t('a, 'e)) => t('b, 'e) =
  (f, onDoneA, onDoneB) =>
    onDoneA(resultA =>
      switch (resultA) {
      | Ok(a) => onDoneB(Ok(f(a)))
      | Error(_) as err => onDoneB(err)
      }
    );

let mapError: ('e1 => 'e2, t('a, 'e1)) => t('a, 'e2) =
  (f, onDone1, onDone2) =>
    onDone1(result1 =>
      switch (result1) {
      | Ok(_) as okA => onDone2(okA)
      | Error(e) => onDone2(Error(f(e)))
      }
    );

let voidError: t('a, 'e1) => t('a, unit) = a => mapError(_ => (), a);

let apply: (t('a => 'b, 'e), t('a, 'e)) => t('b, 'e) =
  (onDoneF, onDoneA, onDoneB) =>
    onDoneF(resultF =>
      switch (resultF) {
      | Ok(f) =>
        onDoneA(resultA =>
          switch (resultA) {
          | Ok(a) => onDoneB(Ok(f(a)))
          | Error(_) as err => onDoneB(err)
          }
        )
      | Error(_) as err => onDoneB(err)
      }
    );

let flatMap: (t('a, 'e), 'a => t('b, 'e)) => t('b, 'e) =
  (onDoneA, aToAffB, onDoneB) =>
    onDoneA(resultA =>
      switch (resultA) {
      | Ok(a) => aToAffB(a, onDoneB)
      | Error(_) as err => onDoneB(err)
      }
    );

module type FUNCTOR_F =
  (Error: BsAbstract.Interface.TYPE) =>
   BsAbstract.Interface.FUNCTOR with type t('a) = t('a, Error.t);

module Functor: FUNCTOR_F =
  (Error: BsAbstract.Interface.TYPE) => {
    type nonrec t('a) = t('a, Error.t);
    let map = map;
  };

module type APPLY_F =
  (Error: BsAbstract.Interface.TYPE) =>
   BsAbstract.Interface.APPLY with type t('a) = t('a, Error.t);

module Apply: APPLY_F =
  (Error: BsAbstract.Interface.TYPE) => {
    include Functor(Error);
    let apply = apply;
  };

module type APPLICATIVE_F =
  (Error: BsAbstract.Interface.TYPE) =>
   BsAbstract.Interface.APPLICATIVE with type t('a) = t('a, Error.t);

module Applicative: APPLICATIVE_F =
  (Error: BsAbstract.Interface.TYPE) => {
    include Apply(Error);
    let pure = pure;
  };

module type MONAD_F =
  (Error: BsAbstract.Interface.TYPE) =>
   BsAbstract.Interface.MONAD with type t('a) = t('a, Error.t);

module Monad: MONAD_F =
  (Error: BsAbstract.Interface.TYPE) => {
    include Applicative(Error);
    let flat_map = flatMap;
  };

/* Infix operators for any arbitrary (given) error type */
module Infix = (Error: BsAbstract.Interface.TYPE) => {
  include BsAbstract.Infix.Monad((Monad(Error)));
};

/* Infix operators for when your error type is Js.Exn.t */
module InfixJsExn =
  Infix({
    type t = Js.Exn.t;
  });