/*---------------------------------------------------------------------------*\

    DAFoam  : Discrete Adjoint with OpenFOAM
    Version : v1.0

\*---------------------------------------------------------------------------*/

#include "tractionDisplacementFvPatchVectorField.H"
#include "addToRunTimeSelectionTable.H"
#include "volFields.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

tractionDisplacementFvPatchVectorField::
tractionDisplacementFvPatchVectorField
(
    const fvPatch& p,
    const DimensionedField<vector, volMesh>& iF
)
:
    fixedGradientFvPatchVectorField(p, iF),
    traction_(p.size(), Zero),
    pressure_(p.size(), 0.0)
{
    fvPatchVectorField::operator=(patchInternalField());
    gradient() = Zero;
}


tractionDisplacementFvPatchVectorField::
tractionDisplacementFvPatchVectorField
(
    const tractionDisplacementFvPatchVectorField& tdpvf,
    const fvPatch& p,
    const DimensionedField<vector, volMesh>& iF,
    const fvPatchFieldMapper& mapper
)
:
    fixedGradientFvPatchVectorField(tdpvf, p, iF, mapper),
    traction_(tdpvf.traction_, mapper),
    pressure_(tdpvf.pressure_, mapper)
{}


tractionDisplacementFvPatchVectorField::
tractionDisplacementFvPatchVectorField
(
    const fvPatch& p,
    const DimensionedField<vector, volMesh>& iF,
    const dictionary& dict
)
:
    fixedGradientFvPatchVectorField(p, iF),
    traction_("traction", dict, p.size()),
    pressure_("pressure", dict, p.size())
{
    fvPatchVectorField::operator=(patchInternalField());
    gradient() = Zero;
}


tractionDisplacementFvPatchVectorField::
tractionDisplacementFvPatchVectorField
(
    const tractionDisplacementFvPatchVectorField& tdpvf
)
:
    fixedGradientFvPatchVectorField(tdpvf),
    traction_(tdpvf.traction_),
    pressure_(tdpvf.pressure_)
{}


tractionDisplacementFvPatchVectorField::
tractionDisplacementFvPatchVectorField
(
    const tractionDisplacementFvPatchVectorField& tdpvf,
    const DimensionedField<vector, volMesh>& iF
)
:
    fixedGradientFvPatchVectorField(tdpvf, iF),
    traction_(tdpvf.traction_),
    pressure_(tdpvf.pressure_)
{}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

void tractionDisplacementFvPatchVectorField::autoMap
(
    const fvPatchFieldMapper& m
)
{
    fixedGradientFvPatchVectorField::autoMap(m);
    traction_.autoMap(m);
    pressure_.autoMap(m);
}


void tractionDisplacementFvPatchVectorField::rmap
(
    const fvPatchVectorField& ptf,
    const labelList& addr
)
{
    fixedGradientFvPatchVectorField::rmap(ptf, addr);

    const tractionDisplacementFvPatchVectorField& dmptf =
        refCast<const tractionDisplacementFvPatchVectorField>(ptf);

    traction_.rmap(dmptf.traction_, addr);
    pressure_.rmap(dmptf.pressure_, addr);
}


void tractionDisplacementFvPatchVectorField::updateCoeffs()
{
    if (updated())
    {
        return;
    }

    const dictionary& mechanicalProperties =
        db().lookupObject<IOdictionary>("mechanicalProperties");

    const dictionary& thermalProperties =
        db().lookupObject<IOdictionary>("thermalProperties");


    const fvPatchField<scalar>& rho =
        patch().lookupPatchField<volScalarField, scalar>("solid:rho");

    const fvPatchField<scalar>& rhoE =
        patch().lookupPatchField<volScalarField, scalar>("E");

    const fvPatchField<scalar>& nu =
        patch().lookupPatchField<volScalarField, scalar>("solid:nu");

    scalarField E(rhoE/rho);
    scalarField mu(E/(2.0*(1.0 + nu)));
    scalarField lambda(nu*E/((1.0 + nu)*(1.0 - 2.0*nu)));
    scalarField threeK(E/(1.0 - 2.0*nu));

    Switch planeStress(mechanicalProperties.lookup("planeStress"));

    if (planeStress)
    {
        lambda = nu*E/((1.0 + nu)*(1.0 - nu));
        threeK = E/(1.0 - nu);
    }

    scalarField twoMuLambda(2*mu + lambda);

    vectorField n(patch().nf());

    // NOTE: we can't use the built-in gradient() computation because
    // it is designed for transient problem, i.e., snGrad() is actually
    // related to gradient()
    // Here we implement our owned BC for steady-state
    const fvPatchField<tensor>& gradD = 
        patch().lookupPatchField<volTensorField, tensor>("gradD");
    gradient() = 
    (
        (traction_ - pressure_*n)/rho
        - (n & (mu*gradD.T() - (mu + lambda)*gradD)) - n*tr(gradD)*lambda
    
    )/(2.0*mu + lambda);

/*
    const fvPatchField<symmTensor>& sigmaD =
        patch().lookupPatchField<volSymmTensorField, symmTensor>("sigmaD");

    gradient() =
    (
        (traction_ - pressure_*n)/rho
      + twoMuLambda*fvPatchField<vector>::snGrad() - (n & sigmaD)
    )/twoMuLambda;

    Info<<"name: "<<patch().name()<<endl;
    Info<<"n: "<<n<<endl;
    Info<<"sigmaD: "<<sigmaD<<endl;
    Info<<"snGrad: "<<fvPatchField<vector>::snGrad()<<endl;
    Info<<"grad: "<<gradient()<<endl; 
*/

    Switch thermalStress(thermalProperties.lookup("thermalStress"));

    if (thermalStress)
    {
        const fvPatchField<scalar>&  threeKalpha=
            patch().lookupPatchField<volScalarField, scalar>("threeKalpha");

        const fvPatchField<scalar>& T =
            patch().lookupPatchField<volScalarField, scalar>("T");

        gradient() += n*threeKalpha*T/twoMuLambda;
    }

    fixedGradientFvPatchVectorField::updateCoeffs();
}


void tractionDisplacementFvPatchVectorField::write(Ostream& os) const
{
    fvPatchVectorField::write(os);
    traction_.writeEntry("traction", os);
    pressure_.writeEntry("pressure", os);
    writeEntry("value", os);
}


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

makePatchTypeField
(
    fvPatchVectorField,
    tractionDisplacementFvPatchVectorField
);

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// ************************************************************************* //
