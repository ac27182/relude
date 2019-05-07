/**
 * Similar to Belt.Result, but has an Applicative instance that collects the errors using a semigroup, rather than fail-fast
 * semantics.
 */
type t('a, 'e) =
  | VOk('a)
  | VError('e);

let pure: 'a 'e. 'a => t('a, 'e) = a => VOk(a);

let ok: 'a 'e. 'a => t('a, 'e) = pure;

let error: 'a 'e. 'e => t('a, 'e) = e => VError(e);

let isOk: 'a 'e. t('a, 'e) => bool =
  fun
  | VOk(_) => true
  | VError(_) => false;

let isError: 'a 'e. t('a, 'e) => bool = a => !isOk(a);

let map: 'a 'b 'e. ('a => 'b, t('a, 'e)) => t('b, 'e) =
  (f, v) =>
    switch (v) {
    | VOk(a) => VOk(f(a))
    | VError(e) => VError(e)
    };

let tap: 'a 'e. ('a => unit, t('a, 'e)) => t('a, 'e) =
  (f, fa) =>
    fa
    |> map(a => {
         f(a);
         a;
       });

let mapError: 'a 'e1 'e2. ('e1 => 'e2, t('a, 'e1)) => t('a, 'e2) =
  (f, v) =>
    switch (v) {
    | VOk(a) => VOk(a)
    | VError(e) => VError(f(e))
    };

let tapError: 'a 'e. ('e => unit, t('a, 'e)) => t('a, 'e) =
  (f, fa) =>
    fa
    |> mapError(e => {
         f(e);
         e;
       });

let bimap: 'a 'b 'e1 'e2. ('a => 'b, 'e1 => 'e2, t('a, 'e1)) => t('b, 'e2) =
  (f, g, fa) =>
    switch (fa) {
    | VOk(a) => VOk(f(a))
    | VError(e) => VError(g(e))
    };

let bitap: 'a 'e. ('a => unit, 'e => unit, t('a, 'e)) => t('a, 'e) =
  (f, g, fa) =>
    fa
    |> bimap(
         a => {
           f(a);
           a;
         },
         e => {
           g(e);
           e;
         },
       );

let apply:
  'a 'b 'e.
  (t('a => 'b, 'e), t('a, 'e), ('e, 'e) => 'e) => t('b, 'e)
 =
  (ff, fv, appendErrors) =>
    switch (ff, fv) {
    | (VOk(f), VOk(a)) => VOk(f(a))
    | (VOk(_), VError(e)) => VError(e)
    | (VError(e), VOk(_)) => VError(e)
    | (VError(e1), VError(e2)) => VError(appendErrors(e1, e2))
    };

/**
 * This function performs a flatMap-like operation, but if the `f` fails, all previous errors are discarded.
 *
 * The function is named flatMapV rather than flatMap to raise awareness of this difference in behavior from
 * what the caller might expect.
 *
 * Also, we do not expose a Monad module for this type for the same reason.
 */
let flatMapV: (t('a, 'e), 'a => t('b, 'e)) => t('b, 'e) =
  (v, f) =>
    switch (v) {
    | VOk(a) => f(a)
    | VError(e) => VError(e)
    };

let fromResult: Belt.Result.t('a, 'b) => t('a, 'b) =
  result =>
    switch (result) {
    | Ok(a) => VOk(a)
    | Error(e) => VError(e)
    };

let toResult: t('a, 'b) => Belt.Result.t('a, 'b) =
  validation =>
    switch (validation) {
    | VOk(a) => Ok(a)
    | VError(a) => Error(a)
    };

let fold: 'a 'e 'c. ('e => 'c, 'a => 'c, t('a, 'e)) => 'c =
  (ec, ac, r) =>
    switch (r) {
    | VOk(a) => ac(a)
    | VError(e) => ec(e)
    };

/**
Flips the values between the success and error channels.
*/
let flip: 'a 'e. t('a, 'e) => t('e, 'a) =
  fa => fa |> fold(e => VOk(e), a => VError(a));

module type FUNCTOR_F =
  (
    Errors: BsAbstract.Interface.SEMIGROUP_ANY,
    Error: BsAbstract.Interface.TYPE,
  ) =>
   BsAbstract.Interface.FUNCTOR with type t('a) = t('a, Errors.t(Error.t));

module Functor: FUNCTOR_F =
  (
    Errors: BsAbstract.Interface.SEMIGROUP_ANY,
    Error: BsAbstract.Interface.TYPE,
  ) => {
    type nonrec t('a) = t('a, Errors.t(Error.t));
    let map = map;
  };

module type APPLY_F =
  (
    Errors: BsAbstract.Interface.SEMIGROUP_ANY,
    Error: BsAbstract.Interface.TYPE,
  ) =>
   BsAbstract.Interface.APPLY with type t('a) = t('a, Errors.t(Error.t));

module Apply: APPLY_F =
  (
    Errors: BsAbstract.Interface.SEMIGROUP_ANY,
    Error: BsAbstract.Interface.TYPE,
  ) => {
    include Functor(Errors, Error);
    let apply = (f, v) => apply(f, v, Errors.append);
  };

module type APPLICATIVE_F =
  (
    Errors: BsAbstract.Interface.SEMIGROUP_ANY,
    Error: BsAbstract.Interface.TYPE,
  ) =>

    BsAbstract.Interface.APPLICATIVE with
      type t('a) = t('a, Errors.t(Error.t));

module Applicative: APPLICATIVE_F =
  (
    Errors: BsAbstract.Interface.SEMIGROUP_ANY,
    Error: BsAbstract.Interface.TYPE,
  ) => {
    include Apply(Errors, Error);
    let pure = pure;
  };

module Infix =
       (
         Errors: BsAbstract.Interface.SEMIGROUP_ANY,
         Error: BsAbstract.Interface.TYPE,
       ) => {
  module Functor = BsAbstract.Infix.Functor((Functor(Errors, Error)));

  module Apply = BsAbstract.Infix.Apply((Apply(Errors, Error)));
};